#include "uno/app/server_config.hpp"
#include "uno/core/build_info.hpp"
#include "uno/net/crypto_runtime.hpp"
#include "uno/net/protocol_version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

TEST_CASE("Project version is a MAJOR.MINOR.PATCH string", "[core][smoke]")
{
    const auto version = uno::core::projectVersion();

    REQUIRE_FALSE(version.empty());
    REQUIRE(std::ranges::count(version, '.') == 2);
}

TEST_CASE("Protocol version is 1", "[net][smoke]")
{
    STATIC_REQUIRE(uno::net::kProtocolVersion == 1);
}

TEST_CASE("Crypto runtime initialises", "[net][smoke]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
}

TEST_CASE("UNO_PORT accepts an integer between 1 and 65535", "[app][config]")
{
    REQUIRE(uno::app::parsePort("1") == 1);
    REQUIRE(uno::app::parsePort("9001") == 9001);
    REQUIRE(uno::app::parsePort("65535") == 65535);
}

TEST_CASE("UNO_PORT rejects values outside the port range", "[app][config]")
{
    using uno::app::ConfigError;

    REQUIRE(uno::app::parsePort("0").error() == ConfigError::PortOutOfRange);
    REQUIRE(uno::app::parsePort("65536").error() == ConfigError::PortOutOfRange);
    REQUIRE(uno::app::parsePort("99999999999999999999999").error() == ConfigError::PortOutOfRange);
}

TEST_CASE("UNO_PORT rejects text that is not a plain integer", "[app][config]")
{
    using uno::app::ConfigError;

    for (const auto* const text : {"", "abc", "90a", " 9001", "-1", "+80", "80.5"}) {
        CAPTURE(text);
        REQUIRE(uno::app::parsePort(text).error() == ConfigError::PortNotANumber);
    }
}
