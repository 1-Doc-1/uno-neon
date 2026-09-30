#include "uno/core/card.hpp"
#include "uno/core/deck.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

using uno::core::Card;
using uno::core::CardId;
using uno::core::Color;
using uno::core::Direction;
using uno::core::DomainError;
using uno::core::kHandSize;
using uno::core::PlayerId;
using uno::core::Rank;
using uno::core::Round;
using uno::core::RoundSetup;
using uno::testing::coloredCard;
using uno::testing::plainCards;
using uno::testing::player;
using uno::testing::players;
using uno::testing::SeededRandomSource;
using uno::testing::wildCard;

namespace {

constexpr std::uint64_t kSeed = 42;

[[nodiscard]] std::size_t dealtCardCount(std::size_t playerCount)
{
    return playerCount * kHandSize;
}

[[nodiscard]] std::vector<Card> shuffledStandardDeck(SeededRandomSource& random)
{
    auto deck = uno::core::createStandardDeck(random);
    uno::core::shuffle(std::span{deck}, random);
    return deck;
}

[[nodiscard]] Round startedRound(RoundSetup setup, SeededRandomSource& random)
{
    auto round = Round::start(std::move(setup), random);
    REQUIRE(round.has_value());
    return *std::move(round);
}

// Deals `playerCount` hands of plain cards, then the given cards follow in draw order.
[[nodiscard]] std::vector<Card> deckFollowingTheHands(std::size_t playerCount, const std::vector<Card>& nextCards)
{
    auto deck = plainCards(dealtCardCount(playerCount));
    std::ranges::copy(nextCards, std::back_inserter(deck));
    return deck;
}

[[nodiscard]] std::span<const Card> handOf(const Round& round, const PlayerId& player)
{
    const auto hand = round.hand(player);
    REQUIRE(hand.has_value());
    return *hand;
}

[[nodiscard]] std::vector<std::uint32_t> idsOf(std::span<const Card> cards)
{
    std::vector<std::uint32_t> ids(cards.size());
    std::ranges::transform(cards, ids.begin(), [](const Card& card) { return card.id.value; });
    return ids;
}

// Every card of the round, wherever it is, as sorted ids.
[[nodiscard]] std::vector<std::uint32_t> allCardIds(const Round& round)
{
    auto ids = idsOf(round.drawPile().cards());
    std::ranges::copy(idsOf(round.discardPile().cards()), std::back_inserter(ids));
    for (const auto& seated : round.seats()) {
        std::ranges::copy(idsOf(handOf(round, seated)), std::back_inserter(ids));
    }
    std::ranges::sort(ids);
    return ids;
}

struct SeatingCase {
    std::vector<PlayerId> seats;
    PlayerId dealer;
    DomainError expected;
};

} // namespace

TEST_CASE("Round rejects an invalid seating", "[core][round]")
{
    const auto seating = GENERATE(values<SeatingCase>({
        {.seats = players(1), .dealer = player(0), .expected = DomainError::NotEnoughPlayers},
        {.seats = players(11), .dealer = player(0), .expected = DomainError::TooManyPlayers},
        {.seats = {player(0), player(0)}, .dealer = player(0), .expected = DomainError::DuplicatePlayer},
        {.seats = players(3), .dealer = player(3), .expected = DomainError::DealerNotSeated},
    }));
    SeededRandomSource random{kSeed};

    const auto round =
        Round::start({.seats = seating.seats, .dealer = seating.dealer, .deck = plainCards(108)}, random);

    REQUIRE(round.error() == seating.expected);
}

TEST_CASE("Round needs exactly 7 cards per player plus one card to flip", "[core][round]")
{
    const auto playerCount = GENERATE(std::size_t{2}, std::size_t{10});
    const auto smallestDeck = dealtCardCount(playerCount) + 1;
    CAPTURE(playerCount, smallestDeck);
    REQUIRE(smallestDeck == (playerCount == 2 ? 15U : 71U));
    SeededRandomSource random{kSeed};

    const auto enough =
        Round::start({.seats = players(playerCount), .dealer = player(0), .deck = plainCards(smallestDeck)}, random);
    const auto tooFew = Round::start(
        {.seats = players(playerCount), .dealer = player(0), .deck = plainCards(smallestDeck - 1)}, random);

    REQUIRE(enough.has_value());
    REQUIRE(tooFew.error() == DomainError::DeckTooSmall);
}

TEST_CASE("Round rejects a deck with duplicate card ids", "[core][round]")
{
    auto deck = plainCards(20);
    deck.back().id = CardId{3};
    SeededRandomSource random{kSeed};

    const auto round = Round::start({.seats = players(2), .dealer = player(0), .deck = deck}, random);

    REQUIRE(round.error() == DomainError::DuplicateCard);
}

TEST_CASE("Each player is dealt 7 cards", "[core][round]")
{
    const auto playerCount = GENERATE(range(std::size_t{2}, std::size_t{11}));
    SeededRandomSource random{kSeed};

    const auto round = startedRound(
        {.seats = players(playerCount), .dealer = player(0), .deck = shuffledStandardDeck(random)}, random);

    for (const auto& seated : round.seats()) {
        REQUIRE(handOf(round, seated).size() == kHandSize);
    }
}

TEST_CASE("Cards are dealt one at a time starting left of the dealer", "[core][round]")
{
    SeededRandomSource random{kSeed};

    const auto round = startedRound({.seats = players(3), .dealer = player(1), .deck = plainCards(30)}, random);

    REQUIRE(idsOf(handOf(round, player(2))) == std::vector<std::uint32_t>{0, 3, 6, 9, 12, 15, 18});
    REQUIRE(idsOf(handOf(round, player(0))) == std::vector<std::uint32_t>{1, 4, 7, 10, 13, 16, 19});
    REQUIRE(idsOf(handOf(round, player(1))) == std::vector<std::uint32_t>{2, 5, 8, 11, 14, 17, 20});
}

TEST_CASE("The next card is flipped onto the discard pile", "[core][round]")
{
    SeededRandomSource random{kSeed};

    const auto round = startedRound({.seats = players(3), .dealer = player(1), .deck = plainCards(30)}, random);

    REQUIRE(round.discardPile().size() == 1);
    REQUIRE(round.discardPile().top().id == CardId{21});
}

TEST_CASE("The draw pile holds the rest of the deck after dealing", "[core][round]")
{
    SeededRandomSource random{kSeed};

    const auto round = startedRound({.seats = players(3), .dealer = player(1), .deck = plainCards(30)}, random);

    REQUIRE(round.drawPile().size() == 30 - dealtCardCount(3) - 1);
    REQUIRE(round.drawPile().cards().back().id == CardId{22});
}

TEST_CASE("Current color is the color of the flipped card", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto deck = deckFollowingTheHands(2, {coloredCard(14, Color::Blue, Rank::Seven)});

    const auto round = startedRound({.seats = players(2), .dealer = player(0), .deck = deck}, random);

    REQUIRE(round.currentColor() == Color::Blue);
}

TEST_CASE("Current color is empty when a wild card is flipped", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto deck = deckFollowingTheHands(2, {wildCard(14, Rank::Wild)});

    const auto round = startedRound({.seats = players(2), .dealer = player(0), .deck = deck}, random);

    REQUIRE(round.discardPile().top().rank == Rank::Wild);
    REQUIRE(round.currentColor() == std::nullopt);
}

TEST_CASE("A Wild Draw Four flipped first goes back into the reshuffled pile and another card is flipped",
          "[core][round]")
{
    const auto seed = GENERATE(std::uint64_t{1}, std::uint64_t{7}, std::uint64_t{2026});
    CAPTURE(seed);
    SeededRandomSource random{seed};
    const auto wildDrawFour = wildCard(14, Rank::WildDrawFour);
    const std::vector nextCards{
        wildDrawFour,
        coloredCard(15, Color::Green, Rank::Two),
        coloredCard(16, Color::Yellow, Rank::Skip),
        coloredCard(17, Color::Blue, Rank::Nine),
    };

    const auto round =
        startedRound({.seats = players(2), .dealer = player(0), .deck = deckFollowingTheHands(2, nextCards)}, random);

    REQUIRE(round.discardPile().size() == 1);
    REQUIRE(round.discardPile().top().rank != Rank::WildDrawFour);
    REQUIRE(round.currentColor() == round.discardPile().top().color);
    REQUIRE(round.drawPile().size() == 3);
    REQUIRE(std::ranges::contains(round.drawPile().cards(), wildDrawFour));
}

TEST_CASE("Flipping continues while the flipped card is a Wild Draw Four", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto greenTwo = coloredCard(18, Color::Green, Rank::Two);
    const std::vector nextCards{
        wildCard(14, Rank::WildDrawFour),
        wildCard(15, Rank::WildDrawFour),
        wildCard(16, Rank::WildDrawFour),
        wildCard(17, Rank::WildDrawFour),
        greenTwo,
    };

    const auto round =
        startedRound({.seats = players(2), .dealer = player(0), .deck = deckFollowingTheHands(2, nextCards)}, random);

    REQUIRE(round.discardPile().top() == greenTwo);
    REQUIRE(round.drawPile().size() == 4);
}

TEST_CASE("Round cannot start when only Wild Draw Four cards are left to flip", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto deck = deckFollowingTheHands(2, {wildCard(14, Rank::WildDrawFour), wildCard(15, Rank::WildDrawFour)});

    const auto round = Round::start({.seats = players(2), .dealer = player(0), .deck = deck}, random);

    REQUIRE(round.error() == DomainError::NoValidStartingCard);
}

TEST_CASE("The player left of the dealer starts and play goes clockwise", "[core][round]")
{
    SeededRandomSource random{kSeed};

    const auto round = startedRound({.seats = players(4), .dealer = player(3), .deck = plainCards(40)}, random);

    REQUIRE(round.dealer() == player(3));
    REQUIRE(round.currentPlayer() == player(0));
    REQUIRE(round.direction() == Direction::Clockwise);
}

TEST_CASE("Dealing conserves every card of the deck", "[core][round]")
{
    const auto playerCount = GENERATE(range(std::size_t{2}, std::size_t{11}));
    CAPTURE(playerCount);
    SeededRandomSource random{kSeed};
    std::vector<std::uint32_t> allIds(uno::core::kStandardDeckSize);
    std::ranges::iota(allIds, std::uint32_t{0});

    const auto round = startedRound(
        {.seats = players(playerCount), .dealer = player(1), .deck = shuffledStandardDeck(random)}, random);

    REQUIRE(allCardIds(round) == allIds);
    REQUIRE(round.drawPile().size() == uno::core::kStandardDeckSize - dealtCardCount(playerCount) - 1);
}

TEST_CASE("Asking for the hand of an unknown player is an error", "[core][round]")
{
    SeededRandomSource random{kSeed};

    const auto round = startedRound({.seats = players(2), .dealer = player(0), .deck = plainCards(20)}, random);

    REQUIRE(round.hand(player(5)).error() == DomainError::UnknownPlayer);
}

TEST_CASE("Starting a round is deterministic for a given seed", "[core][round]")
{
    const auto startWithSeed = [](std::uint64_t seed) {
        SeededRandomSource random{seed};
        return startedRound({.seats = players(4), .dealer = player(2), .deck = shuffledStandardDeck(random)}, random);
    };

    const auto round = startWithSeed(kSeed);

    REQUIRE(round == startWithSeed(kSeed));
    REQUIRE(round != startWithSeed(kSeed + 1));
}
