#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

using uno::core::Card;
using uno::core::CardId;
using uno::core::CardPlayed;
using uno::core::ChooseColor;
using uno::core::Color;
using uno::core::ColorChosen;
using uno::core::DeckReshuffled;
using uno::core::Direction;
using uno::core::DirectionReversed;
using uno::core::DomainError;
using uno::core::DomainEvent;
using uno::core::kHandSize;
using uno::core::PenaltyCardsDrawn;
using uno::core::PlayCard;
using uno::core::PlayerSkipped;
using uno::core::Rank;
using uno::core::TurnChanged;
using uno::testing::coloredCard;
using uno::testing::handOf;
using uno::testing::plainCards;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

namespace {

constexpr std::uint64_t kSeed = 42;

// A hand of 7 plain cards (ids 0..6) with its first card replaced.
[[nodiscard]] std::vector<Card> handWith(Card first)
{
    auto hand = plainCards(kHandSize);
    hand.front() = first;
    return hand;
}

// Deals `currentHand` to player(0) (left of dealer player(playerCount - 1)) and plain filler to
// every other player, flips `top`, then leaves `afterTop` next in the draw pile, in draw order.
// Ids: `currentHand` uses 0..6, this filler uses 100+, `top`/`afterTop` are expected to use ids
// from 40 up, so nothing ever collides.
[[nodiscard]] std::vector<Card> deckFor(std::size_t playerCount, const std::vector<Card>& currentHand, const Card& top,
                                        const std::vector<Card>& afterTop = {})
{
    std::vector<Card> deck;
    deck.reserve((playerCount * kHandSize) + 1 + afterTop.size());
    for (std::size_t i = 0; i < kHandSize; ++i) {
        deck.push_back(currentHand.at(i));
        for (std::size_t seat = 1; seat < playerCount; ++seat) {
            const auto id = 100 + static_cast<std::uint32_t>((seat * kHandSize) + i);
            deck.push_back(coloredCard(id, Color::Red, Rank::Five));
        }
    }
    deck.push_back(top);
    std::ranges::copy(afterTop, std::back_inserter(deck));
    return deck;
}

} // namespace

TEST_CASE("Playing Skip advances the turn past the next player", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Skip));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           PlayerSkipped{.skippedPlayer = player(1)},
                           TurnChanged{.player = player(2)},
                       });
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("Playing Reverse changes the direction of play", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Reverse));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(4), .dealer = player(3), .deck = deckFor(4, hand, top)}, random);
    REQUIRE(round.direction() == Direction::Clockwise);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           DirectionReversed{},
                           TurnChanged{.player = player(3)},
                       });
    REQUIRE(round.direction() == Direction::CounterClockwise);
    REQUIRE(round.currentPlayer() == player(3));
    requireRoundInvariants(round);
}

TEST_CASE("Playing Reverse with two players skips the opponent like a Skip would", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Reverse));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           PlayerSkipped{.skippedPlayer = player(1)},
                           TurnChanged{.player = player(0)},
                       });
    REQUIRE(round.currentPlayer() == player(0));
    requireRoundInvariants(round);
}

TEST_CASE("Playing DrawTwo makes the next player draw two cards and skips their turn", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::DrawTwo));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),
        coloredCard(42, Color::Green, Rank::Two),
    };
    auto round =
        startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top, afterTop)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{42}}},
                           PlayerSkipped{.skippedPlayer = player(1)},
                           TurnChanged{.player = player(2)},
                       });
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 2);
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("Playing DrawTwo reshuffles the discard pile if the draw pile runs out", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::DrawTwo));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto onlyDrawable = coloredCard(41, Color::Green, Rank::One);
    auto round =
        startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top, {onlyDrawable})}, random);
    REQUIRE(round.drawPile().size() == 1);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           DeckReshuffled{},
                           PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{40}}},
                           PlayerSkipped{.skippedPlayer = player(1)},
                           TurnChanged{.player = player(0)},
                       });
    REQUIRE(round.currentPlayer() == player(0));
    requireRoundInvariants(round);
}

TEST_CASE("Playing Wild without a chosen color is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::Wild));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);

    const auto result = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(result.error() == DomainError::ColorRequired);
}

TEST_CASE("Playing Wild sets the current color for subsequent plays", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::Wild));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           ColorChosen{.player = player(0), .color = Color::Green},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.currentColor() == Color::Green);
    requireRoundInvariants(round);
}

TEST_CASE("Choosing a color outside the awaiting-color-choice phase is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = deckFor(2, plainCards(kHandSize), top)}, random);

    const auto result = round.apply(player(0), ChooseColor{.color = Color::Green}, random);

    REQUIRE(result.error() == DomainError::InvalidPhase);
}

TEST_CASE("Every turn change emits a TurnChanged event with the new current player", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Three));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(events->back() == DomainEvent{TurnChanged{.player = player(1)}});
}
