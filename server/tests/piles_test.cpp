#include "uno/core/card.hpp"
#include "uno/core/deck.hpp"
#include "uno/core/piles.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

using uno::core::Card;
using uno::core::CardId;
using uno::core::DiscardPile;
using uno::core::drawCards;
using uno::core::DrawPile;
using uno::testing::plainCards;
using uno::testing::SeededRandomSource;

namespace {

constexpr std::uint64_t kSeed = 42;

[[nodiscard]] std::vector<std::uint32_t> idsOf(std::span<const Card> cards)
{
    return cards | std::views::transform([](const Card& card) { return card.id.value; }) |
           std::ranges::to<std::vector>();
}

[[nodiscard]] std::vector<std::uint32_t> sortedIds(std::vector<std::uint32_t> ids)
{
    std::ranges::sort(ids);
    return ids;
}

// Discard pile holding `cards` from bottom to top (the last one is on top).
[[nodiscard]] DiscardPile discardPileOf(const std::vector<Card>& cards)
{
    DiscardPile pile{cards.front()};
    for (const auto& card : cards | std::views::drop(1)) {
        pile.place(card);
    }
    return pile;
}

// First `count` cards of `cards` go to the draw pile, the rest to the discard pile.
struct Piles {
    DrawPile draw;
    DiscardPile discard;
};

[[nodiscard]] Piles splitPiles(const std::vector<Card>& cards, std::size_t drawCount)
{
    const auto split = cards.begin() + static_cast<std::ptrdiff_t>(drawCount);
    return Piles{
        .draw = DrawPile{std::vector(cards.begin(), split)},
        .discard = discardPileOf(std::vector(split, cards.end())),
    };
}

} // namespace

TEST_CASE("Draw pile deals cards in the given order", "[core][piles]")
{
    const auto cards = plainCards(3);
    DrawPile pile{cards};
    REQUIRE(pile.size() == 3);

    REQUIRE(pile.drawTop() == cards.at(0));
    REQUIRE(pile.drawTop() == cards.at(1));
    REQUIRE(pile.drawTop() == cards.at(2));
    REQUIRE(pile.empty());
}

TEST_CASE("Drawing from an empty draw pile yields nothing", "[core][piles]")
{
    DrawPile pile;

    REQUIRE(pile.drawTop() == std::nullopt);
}

TEST_CASE("Discard pile top is the last card placed", "[core][piles]")
{
    const auto cards = plainCards(2);
    DiscardPile pile{cards.at(0)};
    REQUIRE(pile.top() == cards.at(0));

    pile.place(cards.at(1));

    REQUIRE(pile.top() == cards.at(1));
    REQUIRE(pile.size() == 2);
}

TEST_CASE("Taking the discard pile leaves only its top card", "[core][piles]")
{
    const auto cards = plainCards(3);
    auto pile = discardPileOf(cards);

    REQUIRE(idsOf(pile.takeAllButTop()) == std::vector<std::uint32_t>{0, 1});
    REQUIRE(pile.size() == 1);
    REQUIRE(pile.top() == cards.at(2));
}

TEST_CASE("Drawing reshuffles the discard pile except its top card when the draw pile is empty", "[core][piles]")
{
    auto [draw, discard] = splitPiles(plainCards(4), 0);
    SeededRandomSource random{kSeed};

    const auto result = drawCards(draw, discard, 1, random);

    REQUIRE(result.reshuffled);
    REQUIRE(result.cards.size() == 1);
    REQUIRE(result.cards.at(0).id != CardId{3});
    REQUIRE(draw.size() == 2);
    REQUIRE(discard.size() == 1);
    REQUIRE(discard.top().id == CardId{3});
}

TEST_CASE("Drawing without running out of cards does not reshuffle", "[core][piles]")
{
    auto [draw, discard] = splitPiles(plainCards(4), 2);
    SeededRandomSource random{kSeed};

    const auto result = drawCards(draw, discard, 2, random);

    REQUIRE_FALSE(result.reshuffled);
    REQUIRE(idsOf(result.cards) == std::vector<std::uint32_t>{0, 1});
    REQUIRE(discard.size() == 2);
}

TEST_CASE("A reshuffle can happen in the middle of a multi-card draw", "[core][piles]")
{
    auto [draw, discard] = splitPiles(plainCards(5), 1);
    SeededRandomSource random{kSeed};

    const auto result = drawCards(draw, discard, 3, random);

    REQUIRE(result.reshuffled);
    REQUIRE(result.cards.size() == 3);
    REQUIRE(result.cards.at(0).id == CardId{0});
    REQUIRE(draw.size() == 1);
    REQUIRE(discard.top().id == CardId{4});
}

TEST_CASE("Drawing more cards than available returns only what exists", "[core][piles]")
{
    auto [draw, discard] = splitPiles(plainCards(4), 1);
    SeededRandomSource random{kSeed};

    const auto result = drawCards(draw, discard, 6, random);

    REQUIRE(sortedIds(idsOf(result.cards)) == std::vector<std::uint32_t>{0, 1, 2});
    REQUIRE(draw.empty());
    REQUIRE(discard.size() == 1);
    REQUIRE(discard.top().id == CardId{3});
}

TEST_CASE("Drawing when both piles are exhausted returns nothing without error", "[core][piles]")
{
    auto [draw, discard] = splitPiles(plainCards(1), 0);
    SeededRandomSource random{kSeed};

    const auto result = drawCards(draw, discard, 2, random);

    REQUIRE(result.cards.empty());
    REQUIRE_FALSE(result.reshuffled);
    REQUIRE(discard.size() == 1);
}

TEST_CASE("Drawing never loses or duplicates a card", "[core][piles]")
{
    const auto seed = GENERATE(std::uint64_t{1}, std::uint64_t{7}, std::uint64_t{2026});
    CAPTURE(seed);
    SeededRandomSource random{seed};
    auto [draw, discard] = splitPiles(uno::core::createStandardDeck(random), uno::core::kStandardDeckSize - 1);

    std::vector<std::uint32_t> allIds(uno::core::kStandardDeckSize);
    std::ranges::iota(allIds, std::uint32_t{0});

    // Each turn draws 1 to 4 cards and puts them back on the discard pile, as if they were played.
    for (int turn = 0; turn < 300; ++turn) {
        const auto drawn = drawCards(draw, discard, random.uniform(4) + 1, random);
        for (const auto& card : drawn.cards) {
            discard.place(card);
        }
        auto ids = idsOf(draw.cards());
        std::ranges::copy(idsOf(discard.cards()), std::back_inserter(ids));
        REQUIRE(sortedIds(ids) == allIds);
    }
}

TEST_CASE("Reshuffle is deterministic for a given seed", "[core][piles]")
{
    const auto drawAfterReshuffle = [] {
        auto [draw, discard] = splitPiles(plainCards(20), 0);
        SeededRandomSource random{kSeed};
        return idsOf(drawCards(draw, discard, 19, random).cards);
    };

    const auto firstRun = drawAfterReshuffle();

    REQUIRE(firstRun == drawAfterReshuffle());
    REQUIRE(firstRun != sortedIds(firstRun));
}
