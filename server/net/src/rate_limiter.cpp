#include "uno/net/rate_limiter.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

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

// Four decimal numbers from 0 to 255, nothing else.
std::optional<std::uint32_t> parseIpv4(std::string_view text)
{
    std::uint32_t value = 0;
    for (int part = 0; part < 4; ++part) {
        const auto dot = text.find('.');
        const auto digits = text.substr(0, dot);
        if (digits.empty() || digits.size() > 3 || (part < 3) != (dot != std::string_view::npos)) {
            return std::nullopt;
        }
        unsigned number = 0;
        const auto [parsedUpTo, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
        if (error != std::errc{} || parsedUpTo != digits.data() + digits.size() || number > 255) {
            return std::nullopt;
        }
        value = (value << 8U) | number;
        text = dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);
    }
    return value;
}

using Ipv6 = std::array<std::uint8_t, 16>;

// An IPv6 address as text, in any spelling: compressed ("::1", "fd00::5") or in full as uWebSockets writes it
// ("0000:0000:0000:0000:0000:0000:0000:0001"), possibly ending with a dotted IPv4 ("::ffff:172.29.88.2").
std::optional<Ipv6> parseIpv6(std::string_view text)
{
    std::vector<std::uint16_t> head;
    std::vector<std::uint16_t> tail;
    bool compressed = false;
    bool justCompressed = false; // right after a "::": a third colon is not allowed
    auto* current = &head;
    while (!text.empty()) {
        if (text.starts_with("::")) {
            if (compressed) {
                return std::nullopt;
            }
            compressed = true;
            justCompressed = true;
            current = &tail;
            text.remove_prefix(2);
            continue;
        }
        if (text.front() == ':') {
            if (justCompressed || (current->empty() && !compressed)) {
                return std::nullopt;
            }
            text.remove_prefix(1);
            if (text.empty()) {
                return std::nullopt;
            }
        }
        const auto end = text.find(':');
        const auto group = text.substr(0, end);
        if (group.find('.') != std::string_view::npos) {
            const auto ipv4 = parseIpv4(group);
            if (!ipv4 || end != std::string_view::npos) {
                return std::nullopt;
            }
            current->push_back(static_cast<std::uint16_t>(*ipv4 >> 16U));
            current->push_back(static_cast<std::uint16_t>(*ipv4 & 0xFFFFU));
            break;
        }
        unsigned value = 0;
        const auto [parsedUpTo, error] = std::from_chars(group.data(), group.data() + group.size(), value, 16);
        if (group.empty() || group.size() > 4 || error != std::errc{} || parsedUpTo != group.data() + group.size()) {
            return std::nullopt;
        }
        current->push_back(static_cast<std::uint16_t>(value));
        justCompressed = false;
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end);
    }
    const auto groups = head.size() + tail.size();
    if (compressed ? groups > 7 : groups != 8) {
        return std::nullopt;
    }
    std::vector<std::uint16_t> all = head;
    all.insert(all.end(), 8 - groups, std::uint16_t{0});
    all.insert(all.end(), tail.begin(), tail.end());
    Ipv6 bytes{};
    for (std::size_t index = 0; index < all.size(); ++index) {
        bytes[2 * index] = static_cast<std::uint8_t>(all[index] >> 8U);
        bytes[2 * index + 1] = static_cast<std::uint8_t>(all[index] & 0xFFU);
    }
    return bytes;
}

// ::ffff:a.b.c.d is the IPv4 address a.b.c.d seen through a dual-stack socket.
std::optional<std::uint32_t> mappedIpv4(const Ipv6& address)
{
    constexpr std::array<std::uint8_t, 12> prefix{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    if (!std::equal(prefix.begin(), prefix.end(), address.begin())) {
        return std::nullopt;
    }
    return (std::uint32_t{address[12]} << 24U) | (std::uint32_t{address[13]} << 16U) |
           (std::uint32_t{address[14]} << 8U) | address[15];
}

// The broadest range we accept is a /8: anything wider is "trust everybody" in disguise.
constexpr int kBroadestPrefix = 8;

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
    while (!list.empty()) {
        const auto comma = list.find(',');
        const auto entry = trim(list.substr(0, comma));
        list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
        if (entry.empty()) {
            continue;
        }
        const auto slash = entry.find('/');
        const auto address = entry.substr(0, slash);
        if (const auto ipv4 = parseIpv4(address)) {
            int prefix = 32;
            if (slash != std::string_view::npos) {
                const auto digits = entry.substr(slash + 1);
                const auto [parsedUpTo, error] = std::from_chars(digits.data(), digits.data() + digits.size(), prefix);
                if (digits.empty() || error != std::errc{} || parsedUpTo != digits.data() + digits.size() ||
                    prefix < 0 || prefix > 32) {
                    return std::unexpected("'" + std::string(entry) + "' is not an IPv4 range like 172.29.88.0/28");
                }
                if (prefix < kBroadestPrefix) {
                    return std::unexpected("'" + std::string(entry) +
                                           "' is too broad: that would trust almost every address");
                }
            }
            const std::uint32_t mask = 0xFFFFFFFFU << (32U - static_cast<unsigned>(prefix));
            proxies.ipv4Ranges_.push_back({.network = *ipv4 & mask, .mask = mask});
        } else if (const auto ipv6 = slash == std::string_view::npos ? parseIpv6(address) : std::nullopt) {
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
            return std::ranges::find(ipv6Addresses_, *ipv6) != ipv6Addresses_.end();
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
