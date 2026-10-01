#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

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

// The address rate limits are counted against. Behind a reverse proxy every connection comes from the proxy, so
// its `X-Forwarded-For` header is used, but ONLY when the proxy is declared trusted (UNO_TRUSTED_PROXY): from
// anyone else the header is forgeable. The last entry is the one the trusted proxy appended itself.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): two addresses, but told apart by their names at the call
[[nodiscard]] std::string clientAddress(std::string_view remoteAddress, std::string_view forwardedFor,
                                        bool trustedProxy);

} // namespace uno::net
