#include "uno/core/card.hpp"
#include "uno/core/playability.hpp"
#include "uno/testing/fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

using uno::core::Color;
using uno::core::isPlayable;
using uno::core::isWildDrawFourLegal;
using uno::core::Rank;
using uno::testing::coloredCard;
using uno::testing::plainCards;
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

TEST_CASE("WildDrawFour is always playable, like Wild, regardless of legality", "[core][playability]")
{
    // Whether it was legal to play is only decided later, by a challenge (isWildDrawFourLegal):
    // playing it, legally or as a bluff, is never blocked by isPlayable itself.
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = wildCard(2, Rank::WildDrawFour);

    REQUIRE(isPlayable(candidate, top, Color::Blue));
}

TEST_CASE("WildDrawFour is legal when the hand has no card of the current color", "[core][playability]")
{
    const auto hand = plainCards(7); // red Fives only

    REQUIRE(isWildDrawFourLegal(hand, Color::Blue));
}

TEST_CASE("WildDrawFour is illegal when the hand has a card of the current color", "[core][playability]")
{
    auto hand = plainCards(6);
    hand.push_back(coloredCard(6, Color::Blue, Rank::Nine));

    REQUIRE_FALSE(isWildDrawFourLegal(hand, Color::Blue));
}

TEST_CASE("A Joker in hand does not make WildDrawFour illegal", "[core][playability]")
{
    // Guards against comparing two empty std::optional<Color> as equal: a Joker's color is empty,
    // and must never be mistaken for a match against the (always definite) current color.
    auto hand = plainCards(6); // red Fives: no conflict with the Blue current color used below
    hand.push_back(wildCard(6, Rank::Wild));

    REQUIRE(isWildDrawFourLegal(hand, Color::Blue));
}

TEST_CASE("Playability follows the current color, not the top card's original color", "[core][playability]")
{
    const auto top = coloredCard(1, Color::Blue, Rank::Seven);
    const auto candidate = coloredCard(2, Color::Green, Rank::Three);

    REQUIRE(isPlayable(candidate, top, Color::Green));
    REQUIRE_FALSE(isPlayable(candidate, top, std::nullopt));
}
