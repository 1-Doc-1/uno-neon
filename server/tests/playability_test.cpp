#include "uno/core/card.hpp"
#include "uno/core/playability.hpp"
#include "uno/testing/fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

using uno::core::Color;
using uno::core::isPlayable;
using uno::core::Rank;
using uno::testing::coloredCard;
using uno::testing::wildCard;

TEST_CASE("A card with the same color as the current color is playable", "[core][playability]")
{
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = coloredCard(2, Color::Blue, Rank::Three);

    REQUIRE(isPlayable(candidate, top, Color::Blue));
}

TEST_CASE("A card with the same rank as the top card is playable regardless of color", "[core][playability]")
{
    const auto top = coloredCard(1, Color::Blue, Rank::Skip);
    const auto candidate = coloredCard(2, Color::Red, Rank::Skip);

    REQUIRE(isPlayable(candidate, top, Color::Blue));
}

TEST_CASE("A card with a different color and rank is not playable", "[core][playability]")
{
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = coloredCard(2, Color::Red, Rank::Three);

    REQUIRE_FALSE(isPlayable(candidate, top, Color::Blue));
}

TEST_CASE("Wild is always playable regardless of the top card or current color", "[core][playability]")
{
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = wildCard(2, Rank::Wild);

    REQUIRE(isPlayable(candidate, top, Color::Red));
}

TEST_CASE("WildDrawFour is not playable by the base rule alone", "[core][playability]")
{
    // Step 1.3b adds its extra legality condition and official challenge; until then it is judged
    // by this same base rule, under which it never matches (see playability.hpp).
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = wildCard(2, Rank::WildDrawFour);

    REQUIRE_FALSE(isPlayable(candidate, top, Color::Blue));
}

TEST_CASE("Playability follows the current color, not the top card's original color", "[core][playability]")
{
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = coloredCard(2, Color::Green, Rank::Three);

    REQUIRE(isPlayable(candidate, top, Color::Green));
    REQUIRE_FALSE(isPlayable(candidate, top, std::nullopt));
}
