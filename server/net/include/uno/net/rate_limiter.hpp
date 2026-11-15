#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace uno::net {

using RateClock = std::chrono::steady_clock;

// Per-connection limit on every message (SPEC §9.4): a bucket that holds `capacity` tokens and gets
// `refillPerSecond` more every second. A client may burst, but cannot sustain more than the refill rate.
class TokenBucket {
public:
    // An empty bucket that never refills: only a placeholder for a connection that is not set up yet (uWebSockets
    // wants its per-socket data to be default-constructible).
    TokenBucket() noexcept = default;
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): named at every call site, and the unit is in the name
    TokenBucket(double capacity, double refillPerSecond, RateClock::time_point now) noexcept;

    // Takes one token if there is one.
    [[nodiscard]] bool tryTake(RateClock::time_point now) noexcept;

private:
    double capacity_ = 0;
    double refillPerSecond_ = 0;
    double tokens_ = 0;
    RateClock::time_point last_;
};

// Per-address limit on rarer, costlier requests (room creation, join attempts that could guess codes): at most
// `maxEvents` per `window` for each key.
class SlidingWindowLimiter {
public:
    SlidingWindowLimiter(std::size_t maxEvents, std::chrono::milliseconds window);

    // Records an event for `key` if it stays within the limit; false (and nothing recorded) otherwise.
    [[nodiscard]] bool tryRecord(const std::string& key, RateClock::time_point now);

    // Number of keys currently remembered: keys whose events all left the window are forgotten.
    [[nodiscard]] std::size_t trackedKeys() const noexcept { return events_.size(); }

private:
    void forgetExpiredKeys(RateClock::time_point now);

    std::size_t maxEvents_;
    std::chrono::milliseconds window_;
    std::unordered_map<std::string, std::deque<RateClock::time_point>> events_;
    std::size_t sinceSweep_ = 0;
};

// The addresses a reverse proxy may connect from (UNO_TRUSTED_PROXY_ADDRESSES). Only a connection that comes from one
// of them can tell us, in `X-Forwarded-For`, the real address of a visitor. A default-constructed list trusts nobody.
// IPv4 addresses and CIDR ranges are understood; IPv6 only as exact addresses. A range that would trust (almost) the
// whole Internet is refused: "trust everything" is never a valid configuration.
class TrustedProxies {
public:
    TrustedProxies() = default;

    // The proxy runs beside the server (tunnel mode, ADR 0033): only this machine is trusted.
    [[nodiscard]] static TrustedProxies loopback();

    // A comma-separated list such as "172.29.88.2" or "10.0.0.0/8, 172.29.88.0/28"; the error says what is wrong.
    [[nodiscard]] static std::expected<TrustedProxies, std::string> parse(std::string_view list);

    // Whether a connection from this address (as written by uWebSockets) comes from a trusted proxy.
    [[nodiscard]] bool trusts(std::string_view remoteAddress) const;

    [[nodiscard]] bool empty() const noexcept { return ipv4Ranges_.empty() && ipv6Addresses_.empty(); }

private:
    struct Ipv4Range {
        std::uint32_t network;
        std::uint32_t mask;
    };

    std::vector<Ipv4Range> ipv4Ranges_;
    std::vector<std::array<std::uint8_t, 16>> ipv6Addresses_;
};

// The address rate limits are counted against. Behind a reverse proxy every connection comes from the proxy, so its
// `X-Forwarded-For` header is used, but ONLY when the connection comes from a trusted proxy
// (UNO_TRUSTED_PROXY_ADDRESSES, or this machine alone with UNO_TRUSTED_PROXY): from anyone else the header is
// forgeable, whatever the configuration says. The last entry is the one the trusted proxy appended itself: with a
// single trusted hop between the visitor and the server, it is the address of the visitor.
[[nodiscard]] std::string clientAddress(std::string_view remoteAddress, std::string_view forwardedFor,
                                        const TrustedProxies& trustedProxies);

} // namespace uno::net
