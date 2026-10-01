#include "uno/core/card.hpp"
#include "uno/core/scoring.hpp"
#include "uno/testing/fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using uno::core::Card;
using uno::core::cardPoints;
using uno::core::Color;
using uno::core::handPoints;
using uno::core::Rank;
using uno::testing::coloredCard;
using uno::testing::wildCard;

TEST_CASE("Numbers are worth their face value", "[core][scoring]")
{
    STATIC_REQUIRE(cardPoints(Rank::Zero) == 0);
    STATIC_REQUIRE(cardPoints(Rank::One) == 1);
    STATIC_REQUIRE(cardPoints(Rank::Five) == 5);
    STATIC_REQUIRE(cardPoints(Rank::Nine) == 9);
}

TEST_CASE("Action cards are worth 20 and Wilds 50", "[core][scoring]")
{
    STATIC_REQUIRE(cardPoints(Rank::Skip) == 20);
    STATIC_REQUIRE(cardPoints(Rank::Reverse) == 20);
    STATIC_REQUIRE(cardPoints(Rank::DrawTwo) == 20);
    STATIC_REQUIRE(cardPoints(Rank::Wild) == 50);
    STATIC_REQUIRE(cardPoints(Rank::WildDrawFour) == 50);
}

TEST_CASE("A hand is worth the sum of its cards", "[core][scoring]")
{
    const std::vector<Card> hand{
        coloredCard(0, Color::Red, Rank::Seven),
        coloredCard(1, Color::Blue, Rank::Skip),
        wildCard(2, Rank::WildDrawFour),
        coloredCard(3, Color::Green, Rank::Zero),
    };

    REQUIRE(handPoints(hand) == 77);
    REQUIRE(handPoints({}) == 0);
}
