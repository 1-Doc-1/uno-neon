#include "uno/net/rate_limiter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using namespace std::chrono_literals;
using uno::net::clientAddress;
using uno::net::RateClock;
using uno::net::SlidingWindowLimiter;
using uno::net::TokenBucket;

namespace {

const RateClock::time_point kStart{};

} // namespace

TEST_CASE("A token bucket allows a burst of its capacity, then the refill rate", "[net][rate]")
{
    TokenBucket bucket(20, 10, kStart);

    int allowedAtOnce = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        allowedAtOnce += bucket.tryTake(kStart) ? 1 : 0;
    }
    REQUIRE(allowedAtOnce == 20);

    // 10 tokens per second: after 300 ms, three more.
    int allowedLater = 0;
    for (int attempt = 0; attempt < 10; ++attempt) {
        allowedLater += bucket.tryTake(kStart + 300ms) ? 1 : 0;
    }
    REQUIRE(allowedLater == 3);
}

TEST_CASE("A token bucket never holds more than its capacity", "[net][rate]")
{
    TokenBucket bucket(5, 10, kStart);

    int allowed = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        allowed += bucket.tryTake(kStart + 1h) ? 1 : 0;
    }

    REQUIRE(allowed == 5);
}

TEST_CASE("A sliding window allows N events per window for each key", "[net][rate]")
{
    SlidingWindowLimiter limiter(3, 1min);

    REQUIRE(limiter.tryRecord("a", kStart));
    REQUIRE(limiter.tryRecord("a", kStart + 10s));
    REQUIRE(limiter.tryRecord("a", kStart + 20s));
    REQUIRE_FALSE(limiter.tryRecord("a", kStart + 30s));
    REQUIRE(limiter.tryRecord("b", kStart + 30s)); // another key has its own count
    REQUIRE(limiter.tryRecord("a", kStart + 61s)); // the first event left the window
    REQUIRE_FALSE(limiter.tryRecord("a", kStart + 62s));
}

TEST_CASE("A refused event is not counted against the key", "[net][rate]")
{
    SlidingWindowLimiter limiter(1, 1min);

    REQUIRE(limiter.tryRecord("a", kStart));
    for (int attempt = 1; attempt < 50; ++attempt) {
        REQUIRE_FALSE(limiter.tryRecord("a", kStart + std::chrono::seconds(attempt) / 2));
    }
    REQUIRE(limiter.tryRecord("a", kStart + 61s));
}

TEST_CASE("Keys whose events all expired are forgotten", "[net][rate]")
{
    SlidingWindowLimiter limiter(1, 1min);
    for (int key = 0; key < 1000; ++key) {
        REQUIRE(limiter.tryRecord(std::to_string(key), kStart));
    }

    for (int key = 0; key < 300; ++key) {
        REQUIRE(limiter.tryRecord("late" + std::to_string(key), kStart + 5min));
    }

    REQUIRE(limiter.trackedKeys() < 400);
}

TEST_CASE("The client address is the connection address unless the proxy is trusted", "[net][rate]")
{
    REQUIRE(clientAddress("1.2.3.4", "9.9.9.9", false) == "1.2.3.4");
    REQUIRE(clientAddress("1.2.3.4", "", true) == "1.2.3.4");
    REQUIRE(clientAddress("10.0.0.1", "9.9.9.9", true) == "9.9.9.9");
}

TEST_CASE("Behind a trusted proxy the last forwarded address wins, because the proxy appended it", "[net][rate]")
{
    // Whatever the client claimed comes first; only the entry added by our own proxy can be believed.
    REQUIRE(clientAddress("10.0.0.1", "6.6.6.6, 7.7.7.7 , 9.9.9.9", true) == "9.9.9.9");
    REQUIRE(clientAddress("10.0.0.1", "6.6.6.6,", true) == "10.0.0.1");
}
