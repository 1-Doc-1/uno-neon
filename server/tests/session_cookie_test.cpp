#include "uno/net/session_cookie.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>

// The session cookie (ADR 0036): what the server reads from the browser and what it asks the browser to keep.

using namespace std::chrono_literals;
using uno::app::SessionToken;
using uno::net::sessionCookieHeader;
using uno::net::sessionTokenFromCookie;

namespace {
constexpr std::string_view kTokenText = "Q2hhbmdlTWVQbGVhc2UxMg"; // 22 characters, URL-safe

std::string token()
{
    return std::string{kTokenText};
}
} // namespace

TEST_CASE("The token is read from a Cookie header", "[net][cookie]")
{
    REQUIRE(sessionTokenFromCookie("uno_session=" + token()) == SessionToken{token()});
}

TEST_CASE("It is found among other cookies, whatever the spaces", "[net][cookie]")
{
    REQUIRE(sessionTokenFromCookie("theme=dark; uno_session=" + token() + "; lang=fr") == SessionToken{token()});
    REQUIRE(sessionTokenFromCookie("a=1;uno_session=" + token()) == SessionToken{token()});
    REQUIRE(sessionTokenFromCookie("  uno_session=" + token() + "  ") == SessionToken{token()});
}

TEST_CASE("A missing, misnamed or misshapen cookie gives no token", "[net][cookie]")
{
    REQUIRE_FALSE(sessionTokenFromCookie("").has_value());
    REQUIRE_FALSE(sessionTokenFromCookie("theme=dark").has_value());
    REQUIRE_FALSE(sessionTokenFromCookie("xuno_session=" + token()).has_value());
    REQUIRE_FALSE(sessionTokenFromCookie("uno_session=").has_value());
    REQUIRE_FALSE(sessionTokenFromCookie("uno_session=tooShort").has_value());
    REQUIRE_FALSE(sessionTokenFromCookie("uno_session=" + token() + "X").has_value());
    REQUIRE_FALSE(sessionTokenFromCookie("uno_session=Q2hhbmdlTWVQbGVhc2U+Mg").has_value()); // '+' is not URL-safe
    REQUIRE_FALSE(sessionTokenFromCookie("uno_session=\"" + token() + "\"").has_value());
}

TEST_CASE("The cookie the server sets cannot be read by a script, nor sent by another site", "[net][cookie]")
{
    const std::string header = sessionCookieHeader(SessionToken{token()}, 24h);

    REQUIRE(header.starts_with("uno_session=" + token() + ";"));
    REQUIRE(header.contains("HttpOnly"));
    REQUIRE(header.contains("Secure"));
    REQUIRE(header.contains("SameSite=Strict"));
    REQUIRE(header.contains("Path=/"));
    REQUIRE(header.contains("Max-Age=86400"));
}

TEST_CASE("What the server sets is what it reads back", "[net][cookie]")
{
    REQUIRE(sessionTokenFromCookie(sessionCookieHeader(SessionToken{token()}, 1h)) == SessionToken{token()});
}
