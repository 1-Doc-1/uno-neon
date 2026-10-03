#include "uno/bootstrap/test_hooks.hpp"
#include "uno/net/crypto_runtime.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint32_t> draws(uno::core::RandomSource& random)
{
    std::vector<std::uint32_t> values;
    for (int index = 0; index < 32; ++index) {
        values.push_back(random.uniform(1'000'000));
    }
    return values;
}

const uno::bootstrap::EnvironmentLookup withTestVariables = [](const char* name) -> std::optional<std::string> {
    if (std::string_view{name} == "UNO_TEST_SEED") {
        return "42";
    }
    if (std::string_view{name} == "UNO_TEST_RECONNECT_GRACE_MS") {
        return "1500";
    }
    return std::nullopt;
};

const uno::bootstrap::EnvironmentLookup emptyEnvironment = [](const char*) -> std::optional<std::string> {
    return std::nullopt;
};

} // namespace

TEST_CASE("The random source ignores UNO_TEST_SEED unless the binary was built with test hooks", "[bootstrap]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());

    const auto first = uno::bootstrap::makeRandomSource(withTestVariables);
    const auto second = uno::bootstrap::makeRandomSource(withTestVariables);

#ifdef UNO_ENABLE_TEST_HOOKS
    REQUIRE(draws(*first) == draws(*second)); // the seed is honoured: two servers deal the same cards
#else
    REQUIRE(draws(*first) != draws(*second)); // production: the seed is not even read
#endif
}

TEST_CASE("Without a seed the random source is never reproducible", "[bootstrap]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());

    const auto first = uno::bootstrap::makeRandomSource(emptyEnvironment);
    const auto second = uno::bootstrap::makeRandomSource(emptyEnvironment);

    REQUIRE(draws(*first) != draws(*second));
}

TEST_CASE("The reconnection grace is only shortened by a binary built with test hooks", "[bootstrap]")
{
    const auto timeouts = uno::bootstrap::makeTimeouts(withTestVariables);
    const auto defaults = uno::bootstrap::makeTimeouts(emptyEnvironment);

    REQUIRE(defaults.reconnectGrace == std::chrono::seconds(60));
#ifdef UNO_ENABLE_TEST_HOOKS
    REQUIRE(timeouts.reconnectGrace == std::chrono::milliseconds(1500));
#else
    REQUIRE(timeouts.reconnectGrace == std::chrono::seconds(60));
#endif
}
