#include "uno/core/card.hpp"
#include "uno/core/deck.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <utility>
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

[[nodiscard]] std::size_t countCards(const std::vector<Card>& deck, std::optional<Color> color, Rank rank)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(deck, [&](const Card& card) { return card.color == color && card.rank == rank; }));
}

} // namespace

TEST_CASE("Standard deck has 110 cards: the official 108 and two Wild Draw Five", "[core][deck]")
{
    STATIC_REQUIRE(uno::core::kStandardDeckSize == 110);
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

TEST_CASE("Standard deck has four Wild, four WildDrawFour and two WildDrawFive", "[core][deck]")
{
    const auto deck = deckWithSeed(kSeed);

    REQUIRE(countCards(deck, std::nullopt, Rank::Wild) == 4);
    REQUIRE(countCards(deck, std::nullopt, Rank::WildDrawFour) == 4);
    REQUIRE(countCards(deck, std::nullopt, Rank::WildDrawFive) == 2);
}

TEST_CASE("The composition of the standard deck is the official one plus two Wild Draw Five", "[core][deck]")
{
    const auto composition = uno::core::compositionOf({});

    REQUIRE(composition.perColor == 25);
    REQUIRE(composition.wild == 4);
    REQUIRE(composition.wildDrawFour == 4);
    REQUIRE(composition.wildDrawFive == 2);
    REQUIRE(composition.total() == uno::core::kStandardDeckSize);
}

TEST_CASE("The multipliers scale the Draw Two, the Wild Draw Four and the Wild Draw Five, whatever the combination",
          "[core][deck]")
{
    using uno::core::CardMultiplier;
    const auto drawTwo =
        GENERATE(CardMultiplier::One, CardMultiplier::Two, CardMultiplier::Three, CardMultiplier::Five);
    const auto four = GENERATE(CardMultiplier::One, CardMultiplier::Three, CardMultiplier::Five);
    const auto five = GENERATE(CardMultiplier::One, CardMultiplier::Two, CardMultiplier::Five);
    const uno::core::DeckSettings settings{.drawTwo = drawTwo, .wildDrawFour = four, .wildDrawFive = five};
    CAPTURE(static_cast<int>(drawTwo), static_cast<int>(four), static_cast<int>(five));

    const auto composition = uno::core::compositionOf(settings);
    SeededRandomSource random{kSeed};
    const auto deck = uno::core::createDeck(settings, random);

    // The pure function and the deck it builds agree, and the total is the sum of the parts
    REQUIRE(deck.size() == composition.total());
    REQUIRE(composition.total() == ((4 * (23 + (2 * static_cast<std::size_t>(drawTwo)))) + 4 +
                                    (4 * static_cast<std::size_t>(four)) + (2 * static_cast<std::size_t>(five))));
    REQUIRE(countCards(deck, std::nullopt, Rank::Wild) == 4);
    REQUIRE(countCards(deck, std::nullopt, Rank::WildDrawFour) == composition.wildDrawFour);
    REQUIRE(countCards(deck, std::nullopt, Rank::WildDrawFive) == composition.wildDrawFive);
    for (const auto color : uno::core::kColors) {
        CAPTURE(color);
        const auto ofColor = std::ranges::count_if(deck, [color](const Card& card) { return card.color == color; });
        REQUIRE(std::cmp_equal(ofColor, composition.perColor));
        REQUIRE(countCards(deck, color, Rank::DrawTwo) == 2 * static_cast<std::size_t>(drawTwo));
        REQUIRE(countCards(deck, color, Rank::Zero) == 1);
        REQUIRE(countCards(deck, color, Rank::Skip) == 2);
    }
    std::set<CardId> ids;
    std::ranges::transform(deck, std::inserter(ids, ids.end()), &Card::id);
    REQUIRE(ids.size() == deck.size());
}

TEST_CASE("The largest deck still deals ten players", "[core][deck]")
{
    const uno::core::DeckSettings settings{
        .drawTwo = uno::core::CardMultiplier::Five,
        .wildDrawFour = uno::core::CardMultiplier::Five,
        .wildDrawFive = uno::core::CardMultiplier::Five,
    };

    REQUIRE(uno::core::compositionOf(settings).total() == 166);
    REQUIRE(uno::core::compositionOf(settings).total() > uno::core::kMaxPlayers * uno::core::kHandSize);
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
