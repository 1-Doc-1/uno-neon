#include "uno/core/deck.hpp"
#include "uno/net/crypto_random_source.hpp"
#include "uno/net/crypto_runtime.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>

TEST_CASE("The production random source stays below the bound and reaches every value", "[net][random]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    uno::net::CryptoRandomSource random;
    std::array<unsigned, 6> seen{};

    for (int draw = 0; draw < 6000; ++draw) {
        const auto value = random.uniform(6);
        REQUIRE(value < 6);
        ++seen.at(value);
    }

    // 6000 draws: each face is expected ~1000 times; 600 leaves a margin of many standard deviations.
    REQUIRE(std::ranges::all_of(seen, [](unsigned count) { return count > 600; }));
    REQUIRE(random.uniform(0) == 0);
    REQUIRE(random.uniform(1) == 0);
}

TEST_CASE("Two decks shuffled by the production source differ and each holds 110 distinct cards", "[net][random]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    uno::net::CryptoRandomSource random;

    const auto first = uno::core::createStandardDeck(random);
    const auto second = uno::core::createStandardDeck(random);

    REQUIRE(first.size() == 110);
    REQUIRE(first != second);
    std::set<std::uint32_t> ids;
    for (const auto& card : first) {
        ids.insert(card.id.value);
    }
    REQUIRE(ids.size() == 110);
}
