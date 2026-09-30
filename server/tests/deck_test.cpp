#include "uno/core/card.hpp"
#include "uno/core/deck.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <vector>

using uno::core::Card;
using uno::core::CardId;
using uno::core::Color;
using uno::core::Rank;
using uno::testing::SeededRandomSource;

namespace {

constexpr std::uint64_t kSeed = 42;

[[nodiscard]] std::vector<Card> deckWithSeed(std::uint64_t seed)
{
    SeededRandomSource random{seed};
    return uno::core::createStandardDeck(random);
}

[[nodiscard]] std::ptrdiff_t countCards(const std::vector<Card>& deck, std::optional<Color> color, Rank rank)
{
    return std::ranges::count_if(deck, [&](const Card& card) { return card.color == color && card.rank == rank; });
}

} // namespace

TEST_CASE("Standard deck has 108 cards", "[core][deck]")
{
    STATIC_REQUIRE(uno::core::kStandardDeckSize == 108);
    REQUIRE(deckWithSeed(kSeed).size() == uno::core::kStandardDeckSize);
}

TEST_CASE("Each color has one zero and two of each number from 1 to 9", "[core][deck]")
{
    const auto deck = deckWithSeed(kSeed);

    for (const auto color : uno::core::kColors) {
        CAPTURE(color);
        REQUIRE(countCards(deck, color, Rank::Zero) == 1);
        for (const auto rank : {
                 Rank::One,
                 Rank::Two,
                 Rank::Three,
                 Rank::Four,
                 Rank::Five,
                 Rank::Six,
                 Rank::Seven,
                 Rank::Eight,
                 Rank::Nine,
             }) {
            CAPTURE(rank);
            REQUIRE(countCards(deck, color, rank) == 2);
        }
    }
}

TEST_CASE("Each color has two Skip, two Reverse and two DrawTwo", "[core][deck]")
{
    const auto deck = deckWithSeed(kSeed);

    for (const auto color : uno::core::kColors) {
        CAPTURE(color);
        REQUIRE(countCards(deck, color, Rank::Skip) == 2);
        REQUIRE(countCards(deck, color, Rank::Reverse) == 2);
        REQUIRE(countCards(deck, color, Rank::DrawTwo) == 2);
    }
}

TEST_CASE("Standard deck has four Wild and four WildDrawFour", "[core][deck]")
{
    const auto deck = deckWithSeed(kSeed);

    REQUIRE(countCards(deck, std::nullopt, Rank::Wild) == 4);
    REQUIRE(countCards(deck, std::nullopt, Rank::WildDrawFour) == 4);
}

TEST_CASE("A card has a color if and only if it is not a wild card", "[core][deck]")
{
    for (const auto& card : deckWithSeed(kSeed)) {
        CAPTURE(card.id.value, card.rank);
        REQUIRE(card.color.has_value() == !uno::core::isWild(card.rank));
    }
}

TEST_CASE("Card ids are unique within a deck", "[core][deck]")
{
    const auto deck = deckWithSeed(kSeed);

    std::set<CardId> ids;
    std::ranges::transform(deck, std::inserter(ids, ids.end()), &Card::id);
    REQUIRE(ids.size() == deck.size());
}

TEST_CASE("Card ids do not reveal the card: two seeds give different ids to the same card", "[core][deck]")
{
    // Both decks list the cards in the same canonical order: only the ids differ.
    const auto first = deckWithSeed(kSeed);
    const auto second = deckWithSeed(kSeed + 1);

    const auto differentIds = std::ranges::count_if(std::views::zip(first, second), [](const auto& pair) {
        const auto& [left, right] = pair;
        return left.id != right.id;
    });
    REQUIRE(differentIds > 100);
}

TEST_CASE("Deck composition is identical for every seed", "[core][deck]")
{
    const auto withoutIds = [](std::vector<Card> deck) {
        for (auto& card : deck) {
            card.id = CardId{0};
        }
        return deck;
    };

    const auto reference = withoutIds(deckWithSeed(kSeed));
    for (const std::uint64_t seed : {1U, 2U, 3U, 1'000U}) {
        CAPTURE(seed);
        REQUIRE(withoutIds(deckWithSeed(seed)) == reference);
    }
}
