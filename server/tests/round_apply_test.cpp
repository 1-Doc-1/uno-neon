#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

using uno::core::AwaitingDrawnCardDecision;
using uno::core::AwaitingPlay;
using uno::core::Card;
using uno::core::CardId;
using uno::core::CardPlayed;
using uno::core::CardsDrawn;
using uno::core::Color;
using uno::core::DeckReshuffled;
using uno::core::DomainError;
using uno::core::DomainEvent;
using uno::core::DrawCard;
using uno::core::kHandSize;
using uno::core::Pass;
using uno::core::PlayCard;
using uno::core::Rank;
using uno::core::TurnChanged;
using uno::core::TurnPassed;
using uno::testing::coloredCard;
using uno::testing::handOf;
using uno::testing::plainCards;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;

namespace {

constexpr std::uint64_t kSeed = 42;

// A hand of 7 plain cards (ids 0..6) with its first card replaced: handy to test one specific
// card while keeping the rest of the hand irrelevant.
[[nodiscard]] std::vector<Card> handWith(Card first)
{
    auto hand = plainCards(kHandSize);
    hand.front() = first;
    return hand;
}

// Deals `hand` to player(0) (left of dealer player(1)) and plain filler to player(1), flips `top`,
// then leaves `afterTop` next in the draw pile, in draw order. Ids: `hand` uses 0..6, this filler
// uses 20..26, `top`/`afterTop` are expected to use ids from 40 up, so nothing ever collides.
[[nodiscard]] std::vector<Card> twoPlayerDeck(const std::vector<Card>& hand, const Card& top,
                                              const std::vector<Card>& afterTop = {})
{
    std::vector<Card> deck;
    deck.reserve((2 * kHandSize) + 1 + afterTop.size());
    for (std::size_t i = 0; i < kHandSize; ++i) {
        deck.push_back(hand.at(i));
        deck.push_back(coloredCard(20 + static_cast<std::uint32_t>(i), Color::Red, Rank::Five));
    }
    deck.push_back(top);
    std::ranges::copy(afterTop, std::back_inserter(deck));
    return deck;
}

} // namespace

TEST_CASE("Playing a card out of turn is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top)}, random);

    const auto result = round.apply(player(1), PlayCard{.cardId = CardId{20}, .chosenColor = std::nullopt}, random);

    REQUIRE(result.error() == DomainError::NotYourTurn);
}

TEST_CASE("Playing a card not in hand is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top)}, random);

    const auto result = round.apply(player(0), PlayCard{.cardId = CardId{999}, .chosenColor = std::nullopt}, random);

    REQUIRE(result.error() == DomainError::CardNotInHand);
}

TEST_CASE("Playing a card that matches neither color nor rank is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Green, Rank::Nine));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(hand, top)}, random);

    const auto result = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(result.error() == DomainError::ColorMismatch);
}

TEST_CASE("Playing a colored card removes it from the hand, places it on the discard pile and advances the turn",
          "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Three));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(hand, top)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.discardPile().top().id == CardId{0});
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(handOf(round, player(0)).size() == kHandSize - 1);
    requireRoundInvariants(round);
}

TEST_CASE("Playing a colored card with a chosen color is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Three));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(hand, top)}, random);

    const auto result = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);

    REQUIRE(result.error() == DomainError::ColorNotAllowed);
}

TEST_CASE("Drawing a playable card enters the awaiting-decision phase without ending the turn", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto drawn = coloredCard(41, Color::Blue, Rank::Eight);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top, {drawn})}, random);

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{CardsDrawn{.player = player(0), .cards = {CardId{41}}}});
    REQUIRE(round.currentPlayer() == player(0));
    REQUIRE(std::holds_alternative<AwaitingDrawnCardDecision>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("Drawing an unplayable card ends the turn automatically", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto unplayable = coloredCard(41, Color::Green, Rank::Nine);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top, {unplayable})},
        random);

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardsDrawn{.player = player(0), .cards = {CardId{41}}},
                           TurnPassed{.player = player(0)},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("Drawing when both piles are exhausted ends the turn automatically without error", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top)}, random);
    REQUIRE(round.drawPile().empty());

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardsDrawn{.player = player(0), .cards = {}},
                           TurnPassed{.player = player(0)},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("Drawing again while awaiting a drawn-card decision is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto drawn = coloredCard(41, Color::Blue, Rank::Eight);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top, {drawn})}, random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    const auto result = round.apply(player(0), DrawCard{}, random);

    REQUIRE(result.error() == DomainError::InvalidPhase);
}

TEST_CASE("Passing without having drawn first is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top)}, random);

    const auto result = round.apply(player(0), Pass{}, random);

    REQUIRE(result.error() == DomainError::InvalidPhase);
}

TEST_CASE("Playing a card other than the drawn one while awaiting the decision is rejected", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Three));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto drawn = coloredCard(41, Color::Blue, Rank::Eight);
    auto round =
        startedRound({.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(hand, top, {drawn})}, random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    const auto result = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(result.error() == DomainError::OnlyDrawnCardPlayable);
}

TEST_CASE("Playing the drawn card when it is legal resolves it like any other play", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto drawn = coloredCard(41, Color::Blue, Rank::Eight);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top, {drawn})}, random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{41}, .chosenColor = std::nullopt}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{41}},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.discardPile().top().id == CardId{41});
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("Passing a playable drawn card ends the turn without playing it", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const auto drawn = coloredCard(41, Color::Blue, Rank::Eight);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(plainCards(kHandSize), top, {drawn})}, random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    const auto events = round.apply(player(0), Pass{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           TurnPassed{.player = player(0)},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(handOf(round, player(0)).size() == kHandSize + 1);
    requireRoundInvariants(round);
}

TEST_CASE("Drawing when the draw pile is empty reshuffles the discard pile and emits DeckReshuffled", "[core][round]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(coloredCard(0, Color::Blue, Rank::Three));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = twoPlayerDeck(hand, top)}, random);
    REQUIRE(round.drawPile().empty());
    REQUIRE(round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random).has_value());

    const auto events = round.apply(player(1), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           DeckReshuffled{},
                           CardsDrawn{.player = player(1), .cards = {CardId{40}}},
                       });
    REQUIRE(std::holds_alternative<AwaitingDrawnCardDecision>(round.phase()));
    requireRoundInvariants(round);
}
