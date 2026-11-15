#include "uno/net/rate_limiter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace uno::net {
namespace {

constexpr std::size_t kSweepEvery = 256; // events between two sweeps for expired keys

void dropOlderThan(std::deque<RateClock::time_point>& events, RateClock::time_point limit)
{
    while (!events.empty() && events.front() <= limit) {
        events.pop_front();
    }
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

std::vector<std::string_view> splitOn(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    for (;;) {
        const auto at = text.find(separator);
        parts.push_back(text.substr(0, at));
        if (at == std::string_view::npos) {
            return parts;
        }
        text.remove_prefix(at + 1);
    }
}

// A number written in `base` with nothing around it, at most `maximum`.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): named at every call site
std::optional<unsigned> parseNumber(std::string_view digits, int base, unsigned maximum)
{
    unsigned value = 0;
    const char* const begin = std::to_address(digits.begin());
    const char* const end = std::to_address(digits.end());
    const auto [parsedUpTo, error] = std::from_chars(begin, end, value, base);
    if (digits.empty() || error != std::errc{} || parsedUpTo != end || value > maximum) {
        return std::nullopt;
    }
    return value;
}

// Four decimal numbers from 0 to 255, nothing else.
std::optional<std::uint32_t> parseIpv4(std::string_view text)
{
    constexpr std::size_t kParts = 4;
    constexpr std::size_t kMaxDigits = 3;
    const auto parts = splitOn(text, '.');
    if (parts.size() != kParts) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (const auto part : parts) {
        const auto number = part.size() <= kMaxDigits ? parseNumber(part, 10, 255) : std::nullopt;
        if (!number) {
            return std::nullopt;
        }
        value = (value << 8U) | *number;
    }
    return value;
}

using Ipv6 = std::array<std::uint8_t, 16>;

// The 16-bit groups of one side of an IPv6 address ("fd00:5", or "ffff:172.29.88.2" with a dotted IPv4 at the end).
// No text at all is no group at all.
std::optional<std::vector<std::uint16_t>> parseGroups(std::string_view text)
{
    std::vector<std::uint16_t> groups;
    std::string_view hex = text;
    std::optional<std::uint32_t> dotted;
    if (text.contains('.')) {
        const auto cut = text.rfind(':');
        dotted = parseIpv4(cut == std::string_view::npos ? text : text.substr(cut + 1));
        if (!dotted) {
            return std::nullopt;
        }
        hex = cut == std::string_view::npos ? std::string_view{} : text.substr(0, cut);
    }
    if (!hex.empty()) {
        for (const auto part : splitOn(hex, ':')) {
            const auto value = part.size() <= 4 ? parseNumber(part, 16, 0xFFFFU) : std::nullopt;
            if (!value) {
                return std::nullopt;
            }
            groups.push_back(static_cast<std::uint16_t>(*value));
        }
    }
    if (dotted) {
        groups.push_back(static_cast<std::uint16_t>(*dotted >> 16U));
        groups.push_back(static_cast<std::uint16_t>(*dotted & 0xFFFFU));
    }
    return groups;
}

// An IPv6 address as text, in any spelling: compressed ("::1", "fd00::5") or in full as uWebSockets writes it
// ("0000:0000:0000:0000:0000:0000:0000:0001").
std::optional<Ipv6> parseIpv6(std::string_view text)
{
    constexpr std::size_t kGroups = 8;
    const auto gap = text.find("::");
    const bool compressed = gap != std::string_view::npos;
    if (compressed && text.find("::", gap + 1) != std::string_view::npos) {
        return std::nullopt; // two compressions, or three colons in a row
    }
    const auto head = parseGroups(text.substr(0, gap));
    const auto tail = compressed ? parseGroups(text.substr(gap + 2)) : parseGroups(std::string_view{});
    if (!head || !tail) {
        return std::nullopt;
    }
    const auto present = head->size() + tail->size();
    if (compressed ? present >= kGroups : present != kGroups) {
        return std::nullopt;
    }
    std::vector<std::uint16_t> groups = *head;
    groups.insert(groups.end(), kGroups - present, std::uint16_t{0});
    groups.insert(groups.end(), tail->begin(), tail->end());

    Ipv6 bytes{};
    for (std::size_t index = 0; index < kGroups; ++index) {
        bytes.at(2 * index) = static_cast<std::uint8_t>(groups.at(index) >> 8U);
        bytes.at((2 * index) + 1) = static_cast<std::uint8_t>(groups.at(index) & 0xFFU);
    }
    return bytes;
}

// ::ffff:a.b.c.d is the IPv4 address a.b.c.d seen through a dual-stack socket.
std::optional<std::uint32_t> mappedIpv4(const Ipv6& address)
{
    constexpr std::array<std::uint8_t, 12> kMappedPrefix{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    if (!std::equal(kMappedPrefix.begin(), kMappedPrefix.end(), address.begin())) {
        return std::nullopt;
    }
    return (std::uint32_t{address.at(12)} << 24U) | (std::uint32_t{address.at(13)} << 16U) |
           (std::uint32_t{address.at(14)} << 8U) | std::uint32_t{address.at(15)};
}

// The broadest range we accept is a /8: anything wider is "trust everybody" in disguise.
constexpr unsigned kBroadestPrefix = 8;

struct Ipv4Block {
    std::uint32_t network;
    std::uint32_t mask;
};

// "172.29.88.2" or "172.29.88.0/28": the address, then the width of the range in bits.
std::expected<Ipv4Block, std::string> parseIpv4Block(std::string_view entry, std::uint32_t address)
{
    unsigned prefix = 32;
    const auto slash = entry.find('/');
    if (slash != std::string_view::npos) {
        const auto width = parseNumber(entry.substr(slash + 1), 10, 32);
        if (!width) {
            return std::unexpected("'" + std::string(entry) + "' is not an IPv4 range like 172.29.88.0/28");
        }
        if (*width < kBroadestPrefix) {
            return std::unexpected("'" + std::string(entry) + "' is too broad: that would trust almost every address");
        }
        prefix = *width;
    }
    const std::uint32_t mask = 0xFFFFFFFFU << (32U - prefix);
    return Ipv4Block{.network = address & mask, .mask = mask};
}

} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
TokenBucket::TokenBucket(double capacity, double refillPerSecond, RateClock::time_point now) noexcept
    : capacity_(capacity), refillPerSecond_(refillPerSecond), tokens_(capacity), last_(now)
{
}

bool TokenBucket::tryTake(RateClock::time_point now) noexcept
{
    const std::chrono::duration<double> elapsed = now - last_;
    last_ = std::max(last_, now);
    tokens_ = std::min(capacity_, tokens_ + (std::max(elapsed.count(), 0.0) * refillPerSecond_));
    if (tokens_ < 1.0) {
        return false;
    }
    tokens_ -= 1.0;
    return true;
}

SlidingWindowLimiter::SlidingWindowLimiter(std::size_t maxEvents, std::chrono::milliseconds window)
    : maxEvents_(maxEvents), window_(window)
{
}

bool SlidingWindowLimiter::tryRecord(const std::string& key, RateClock::time_point now)
{
    if (++sinceSweep_ >= kSweepEvery) {
        forgetExpiredKeys(now);
    }
    auto& events = events_[key];
    dropOlderThan(events, now - window_);
    if (events.size() >= maxEvents_) {
        return false;
    }
    events.push_back(now);
    return true;
}

void SlidingWindowLimiter::forgetExpiredKeys(RateClock::time_point now)
{
    sinceSweep_ = 0;
    std::erase_if(events_, [&](auto& entry) {
        dropOlderThan(entry.second, now - window_);
        return entry.second.empty();
    });
}

TrustedProxies TrustedProxies::loopback()
{
    TrustedProxies proxies;
    proxies.ipv4Ranges_.push_back({.network = 0x7F000001U, .mask = 0xFFFFFFFFU});
    proxies.ipv6Addresses_.push_back(Ipv6{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    return proxies;
}

std::expected<TrustedProxies, std::string> TrustedProxies::parse(std::string_view list)
{
    TrustedProxies proxies;
    for (const auto item : splitOn(list, ',')) {
        const auto entry = trim(item);
        if (entry.empty()) {
            continue;
        }
        const auto slash = entry.find('/');
        const auto address = entry.substr(0, slash);
        if (const auto ipv4 = parseIpv4(address)) {
            const auto block = parseIpv4Block(entry, *ipv4);
            if (!block) {
                return std::unexpected(block.error());
            }
            proxies.ipv4Ranges_.push_back({.network = block->network, .mask = block->mask});
        } else if (const auto ipv6 = slash == std::string_view::npos ? parseIpv6(address) : std::optional<Ipv6>{}) {
            if (const auto mapped = mappedIpv4(*ipv6)) {
                proxies.ipv4Ranges_.push_back({.network = *mapped, .mask = 0xFFFFFFFFU});
            } else {
                proxies.ipv6Addresses_.push_back(*ipv6);
            }
        } else {
            return std::unexpected(
                "'" + std::string(entry) +
                "' is not an IPv4 address or range, nor an exact IPv6 address (IPv6 ranges are not supported)");
        }
    }
    if (proxies.empty()) {
        return std::unexpected("the list of trusted proxies is empty");
    }
    return proxies;
}

bool TrustedProxies::trusts(std::string_view remoteAddress) const
{
    std::optional<std::uint32_t> ipv4 = parseIpv4(remoteAddress);
    if (!ipv4) {
        const auto ipv6 = parseIpv6(remoteAddress);
        if (!ipv6) {
            return false;
        }
        ipv4 = mappedIpv4(*ipv6);
        if (!ipv4) {
            return std::ranges::contains(ipv6Addresses_, *ipv6);
        }
    }
    return std::ranges::any_of(ipv4Ranges_,
                               [&](const Ipv4Range& range) { return (*ipv4 & range.mask) == range.network; });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::string clientAddress(std::string_view remoteAddress, std::string_view forwardedFor,
                          const TrustedProxies& trustedProxies)
{
    if (trustedProxies.trusts(remoteAddress) && !forwardedFor.empty()) {
        const auto lastComma = forwardedFor.rfind(',');
        const auto last = trim(lastComma == std::string_view::npos ? forwardedFor : forwardedFor.substr(lastComma + 1));
        if (!last.empty()) {
            return std::string(last);
        }
    }
    return std::string(remoteAddress);
}

} // namespace uno::net
