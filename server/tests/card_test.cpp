#include "uno/core/card.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>

using uno::core::CardId;
using uno::core::Rank;

TEST_CASE("CardId is a distinct type, not convertible from an integer", "[core][card]")
{
    STATIC_REQUIRE_FALSE(std::is_convertible_v<std::uint32_t, CardId>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<CardId, std::uint32_t>);
    STATIC_REQUIRE(CardId{3} == CardId{3});
    STATIC_REQUIRE(CardId{3} < CardId{4});
}

TEST_CASE("Wild and WildDrawFour are the only wild ranks", "[core][card]")
{
    STATIC_REQUIRE(uno::core::isWild(Rank::Wild));
    STATIC_REQUIRE(uno::core::isWild(Rank::WildDrawFour));

    constexpr std::array kColoredRanks{
        Rank::Zero,  Rank::One,   Rank::Two,  Rank::Three, Rank::Four,    Rank::Five,    Rank::Six,
        Rank::Seven, Rank::Eight, Rank::Nine, Rank::Skip,  Rank::Reverse, Rank::DrawTwo,
    };
    STATIC_REQUIRE(std::ranges::none_of(kColoredRanks, uno::core::isWild));
}
