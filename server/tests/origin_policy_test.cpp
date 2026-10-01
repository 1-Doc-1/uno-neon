#include "uno/app/server_config.hpp"
#include "uno/net/origin_policy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

TEST_CASE("An origin is allowed only when listed, ignoring case", "[net][origin]")
{
    const uno::net::OriginPolicy policy({"https://uno.example.com", "http://localhost:4200"});

    REQUIRE(policy.allows("https://uno.example.com"));
    REQUIRE(policy.allows("HTTPS://UNO.EXAMPLE.COM"));
    REQUIRE(policy.allows("http://localhost:4200"));
    REQUIRE_FALSE(policy.allows("https://evil.example.com"));
    REQUIRE_FALSE(policy.allows("https://uno.example.com.evil.com"));
    REQUIRE_FALSE(policy.allows("http://uno.example.com"));
    REQUIRE_FALSE(policy.allows("http://localhost:4201"));
}

TEST_CASE("A missing Origin header is refused", "[net][origin]")
{
    REQUIRE_FALSE(uno::net::OriginPolicy({"https://uno.example.com"}).allows(""));
    REQUIRE_FALSE(uno::net::OriginPolicy({}).allows(""));
}

TEST_CASE("UNO_ALLOWED_ORIGINS is a comma-separated list of origins", "[app][config]")
{
    const auto origins = uno::app::parseAllowedOrigins(" https://uno.example.com , http://localhost:4200,");

    REQUIRE(origins.has_value());
    REQUIRE(*origins == std::vector<std::string>{"https://uno.example.com", "http://localhost:4200"});
}

TEST_CASE("UNO_ALLOWED_ORIGINS rejects anything that is not an origin", "[app][config]")
{
    for (const char* text : {
             "",
             ",",
             "https://a.com,,https://b.com",
             "uno.example.com",
             "ftp://uno.example.com",
             "https://uno.example.com/",
             "https://uno.example.com/path",
             "https://",
             "*",
         }) {
        REQUIRE(uno::app::parseAllowedOrigins(text).error() == uno::app::ConfigError::OriginInvalid);
    }
}

TEST_CASE("Without UNO_ALLOWED_ORIGINS only the dev server of the client is allowed", "[app][config]")
{
    const uno::app::ServerConfig config;

    REQUIRE(config.allowedOrigins == uno::app::defaultAllowedOrigins());
    REQUIRE(uno::net::OriginPolicy(config.allowedOrigins).allows("http://localhost:4200"));
}
