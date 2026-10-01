#include "uno/net/rate_limiter.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::string clientAddress(std::string_view remoteAddress, std::string_view forwardedFor, bool trustedProxy)
{
    if (trustedProxy && !forwardedFor.empty()) {
        const auto lastComma = forwardedFor.rfind(',');
        const auto last = trim(lastComma == std::string_view::npos ? forwardedFor : forwardedFor.substr(lastComma + 1));
        if (!last.empty()) {
            return std::string(last);
        }
    }
    return std::string(remoteAddress);
}

} // namespace uno::net
