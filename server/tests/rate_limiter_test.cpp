#include "uno/net/rate_limiter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

using namespace std::chrono_literals;
using uno::net::clientAddress;
using uno::net::RateClock;
using uno::net::SlidingWindowLimiter;
using uno::net::TokenBucket;
using uno::net::TrustedProxies;

namespace {

RateClock::time_point start()
{
    return RateClock::time_point{};
}

} // namespace

TEST_CASE("A token bucket allows a burst of its capacity, then the refill rate", "[net][rate]")
{
    TokenBucket bucket(20, 10, start());

    int allowedAtOnce = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        allowedAtOnce += bucket.tryTake(start()) ? 1 : 0;
    }
    REQUIRE(allowedAtOnce == 20);

    // 10 tokens per second: after 300 ms, three more.
    int allowedLater = 0;
    for (int attempt = 0; attempt < 10; ++attempt) {
        allowedLater += bucket.tryTake(start() + 300ms) ? 1 : 0;
    }
    REQUIRE(allowedLater == 3);
}

TEST_CASE("A token bucket never holds more than its capacity", "[net][rate]")
{
    TokenBucket bucket(5, 10, start());

    int allowed = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        allowed += bucket.tryTake(start() + 1h) ? 1 : 0;
    }

    REQUIRE(allowed == 5);
}

TEST_CASE("A sliding window allows N events per window for each key", "[net][rate]")
{
    SlidingWindowLimiter limiter(3, 1min);

    REQUIRE(limiter.tryRecord("a", start()));
    REQUIRE(limiter.tryRecord("a", start() + 10s));
    REQUIRE(limiter.tryRecord("a", start() + 20s));
    REQUIRE_FALSE(limiter.tryRecord("a", start() + 30s));
    REQUIRE(limiter.tryRecord("b", start() + 30s)); // another key has its own count
    REQUIRE(limiter.tryRecord("a", start() + 61s)); // the first event left the window
    REQUIRE_FALSE(limiter.tryRecord("a", start() + 62s));
}

TEST_CASE("A refused event is not counted against the key", "[net][rate]")
{
    SlidingWindowLimiter limiter(1, 1min);

    REQUIRE(limiter.tryRecord("a", start()));
    for (int attempt = 1; attempt < 50; ++attempt) {
        REQUIRE_FALSE(limiter.tryRecord("a", start() + std::chrono::seconds(attempt) / 2));
    }
    REQUIRE(limiter.tryRecord("a", start() + 61s));
}

TEST_CASE("Keys whose events all expired are forgotten", "[net][rate]")
{
    SlidingWindowLimiter limiter(1, 1min);
    for (int key = 0; key < 1000; ++key) {
        REQUIRE(limiter.tryRecord(std::to_string(key), start()));
    }

    for (int key = 0; key < 300; ++key) {
        REQUIRE(limiter.tryRecord("late" + std::to_string(key), start() + 5min));
    }

    REQUIRE(limiter.trackedKeys() < 400);
}

TEST_CASE("The client address is the connection address unless the proxy is trusted", "[net][rate]")
{
    const auto loopback = TrustedProxies::loopback();
    REQUIRE(clientAddress("1.2.3.4", "9.9.9.9", TrustedProxies{}) == "1.2.3.4");
    REQUIRE(clientAddress("127.0.0.1", "9.9.9.9", TrustedProxies{}) == "127.0.0.1");
    REQUIRE(clientAddress("1.2.3.4", "", loopback) == "1.2.3.4");
    REQUIRE(clientAddress("127.0.0.1", "9.9.9.9", loopback) == "9.9.9.9");
    REQUIRE(clientAddress("::1", "9.9.9.9", loopback) == "9.9.9.9");
}

TEST_CASE("The forwarded address is only believed when the connection comes from this machine", "[net][rate]")
{
    // Caddy runs beside the server (ADR 0036): from anybody else the header is a forgery, whatever the configuration
    const auto loopback = TrustedProxies::loopback();
    REQUIRE(clientAddress("10.0.0.1", "9.9.9.9", loopback) == "10.0.0.1");
    REQUIRE(clientAddress("203.0.113.7", "127.0.0.1", loopback) == "203.0.113.7");
}

TEST_CASE("Behind a trusted proxy the last forwarded address wins, because the proxy appended it", "[net][rate]")
{
    // Whatever the client claimed comes first; only the entry added by our own proxy can be believed.
    const auto loopback = TrustedProxies::loopback();
    REQUIRE(clientAddress("127.0.0.1", "6.6.6.6, 7.7.7.7 , 9.9.9.9", loopback) == "9.9.9.9");
    REQUIRE(clientAddress("127.0.0.1", "6.6.6.6,", loopback) == "127.0.0.1");
}

// ---- UNO_TRUSTED_PROXY_ADDRESSES (ADR 0044) : le conteneur Caddy, et lui seul ----

TEST_CASE("A trusted proxy is an exact address or a range, and only its connections are believed", "[net][rate][proxy]")
{
    const auto proxies = TrustedProxies::parse("172.29.88.2, 10.1.0.0/16");
    REQUIRE(proxies.has_value());

    REQUIRE(clientAddress("172.29.88.2", "203.0.113.9", *proxies) == "203.0.113.9");
    REQUIRE(clientAddress("10.1.200.3", "203.0.113.9", *proxies) == "203.0.113.9");
    // The neighbours of the proxy are not the proxy: their header is a forgery
    REQUIRE(clientAddress("172.29.88.3", "203.0.113.9", *proxies) == "172.29.88.3");
    REQUIRE(clientAddress("10.2.0.1", "203.0.113.9", *proxies) == "10.2.0.1");
    // An explicit list replaces the loopback: this machine is trusted only if it is listed
    REQUIRE(clientAddress("127.0.0.1", "203.0.113.9", *proxies) == "127.0.0.1");
}

TEST_CASE("An IPv4-mapped IPv6 connection address is read as the IPv4 address", "[net][rate][proxy]")
{
    const auto proxies = TrustedProxies::parse("172.29.88.2");
    REQUIRE(proxies.has_value());
    REQUIRE(clientAddress("::ffff:172.29.88.2", "203.0.113.9", *proxies) == "203.0.113.9");
    REQUIRE(clientAddress("::ffff:172.29.88.9", "203.0.113.9", *proxies) == "::ffff:172.29.88.9");
}

TEST_CASE("Exact IPv6 addresses can be trusted, whatever the spelling", "[net][rate][proxy]")
{
    const auto proxies = TrustedProxies::parse("0:0:0:0:0:0:0:1, FD00::5");
    REQUIRE(proxies.has_value());
    REQUIRE(proxies->trusts("::1"));
    // uWebSockets writes IPv6 addresses in full, in groups of four digits
    REQUIRE(proxies->trusts("0000:0000:0000:0000:0000:0000:0000:0001"));
    REQUIRE(proxies->trusts("fd00::5"));
    REQUIRE(proxies->trusts("fd00:0000:0000:0000:0000:0000:0000:0005"));
    REQUIRE_FALSE(proxies->trusts("fd00::6"));
    REQUIRE_FALSE(proxies->trusts("fd00:0000:0000:0000:0000:0000:0000:0006"));
}

TEST_CASE("A dual-stack connection from a trusted IPv4 proxy is recognised, as uWebSockets writes it",
          "[net][rate][proxy]")
{
    const auto proxies = TrustedProxies::parse("172.29.88.2");
    REQUIRE(proxies.has_value());
    // 172.29.88.2 is ac1d:5802
    REQUIRE(proxies->trusts("0000:0000:0000:0000:0000:ffff:ac1d:5802"));
    REQUIRE_FALSE(proxies->trusts("0000:0000:0000:0000:0000:ffff:ac1d:5803"));
    // The loopback list trusts 127.0.0.1 seen through a dual-stack socket too
    REQUIRE(TrustedProxies::loopback().trusts("0000:0000:0000:0000:0000:ffff:7f00:0001"));
    REQUIRE_FALSE(TrustedProxies::loopback().trusts("0000:0000:0000:0000:0000:ffff:7f00:0002"));
}

TEST_CASE("Addresses that are not addresses are trusted by nobody", "[net][rate][proxy]")
{
    const auto proxies = TrustedProxies::parse("172.29.88.2, ::1");
    REQUIRE(proxies.has_value());
    for (const char* text : {"", "caddy", "172.29.88", "172.29.88.2.1", "1::2::3", ":::1", "::1:", "12345::1", "::g"}) {
        INFO("text: '" << text << "'");
        REQUIRE_FALSE(proxies->trusts(text));
    }
}

TEST_CASE("Trusting everybody is not a configuration: broad ranges, nonsense and empty lists are refused",
          "[net][rate][proxy]")
{
    for (const char* list : {"0.0.0.0/0", "10.0.0.0/7", "0.0.0.0/1", "", " , ", "172.29.88.256", "172.29.88.0/33",
                             "172.29.88.0/", "172.29.88", "*", "all", "true", "fd00::/8", "caddy", "172.29.88.2/abc"}) {
        INFO("list: '" << list << "'");
        REQUIRE_FALSE(TrustedProxies::parse(list).has_value());
    }
    REQUIRE(TrustedProxies::parse("10.0.0.0/8").has_value());
}

TEST_CASE("Nobody is trusted by default", "[net][rate][proxy]")
{
    const TrustedProxies nobody;
    REQUIRE(nobody.empty());
    REQUIRE_FALSE(nobody.trusts("127.0.0.1"));
    REQUIRE_FALSE(nobody.trusts("172.29.88.2"));
}

TEST_CASE("Behind Traefik then Caddy, the server sees one hop: the visitor Caddy wrote, never what the visitor claimed",
          "[net][rate][proxy]")
{
    // Caddy trusts Traefik, reads the visitor from X-Forwarded-For and REWRITES the header with that single address;
    // the server trusts only Caddy's own address. A visitor who sends "X-Forwarded-For: 1.2.3.4" reaches Caddy as
    // "1.2.3.4, <their real address>" and Caddy forwards the real one alone.
    const auto caddy = TrustedProxies::parse("172.29.88.2");
    REQUIRE(caddy.has_value());
    REQUIRE(clientAddress("172.29.88.2", "198.51.100.23", *caddy) == "198.51.100.23");
    // Anything else on the network that talks to the server directly cannot choose its own address
    REQUIRE(clientAddress("172.29.88.9", "198.51.100.23", *caddy) == "172.29.88.9");
}
