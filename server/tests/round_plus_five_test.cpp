#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
#include "uno/core/playability.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"
#include "uno/core/scoring.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// The Wild Draw Five (ADR 0028): playable on anything, it names a colour and a target; the target accepts and draws,
// or answers with a Wild Draw Five of their own and the total grows by five.

using namespace uno::core;
using uno::testing::coloredCard;
using uno::testing::deckFromHands;
using uno::testing::handOf;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

namespace {

constexpr std::uint64_t kSeed = 42;

[[nodiscard]] Card plusFive(std::uint32_t id)
{
    return wildCard(id, Rank::WildDrawFive);
}

// A hand of seven cards: `special` first, then red Fives with ids from `firstId`.
[[nodiscard]] std::vector<Card> sevenCards(std::uint32_t firstId, const std::vector<Card>& special)
{
    std::vector<Card> hand = special;
    for (auto slot = static_cast<std::uint32_t>(special.size()); slot < kHandSize; ++slot) {
        hand.push_back(coloredCard(firstId + slot, Color::Red, Rank::Five));
    }
    return hand;
}

// `special` lists, seat by seat, the cards each hand starts with (the rest of each hand is red Fives, ids 1000 x
// (seat + 1) and up). Seat 0 plays first (the last seat deals); a blue Two is flipped, `afterTop` follows.
[[nodiscard]] Round table(const std::vector<std::vector<Card>>& special, SeededRandomSource& random,
                          const std::vector<Card>& afterTop = {}, std::size_t count = 4,
                          DrawRule drawRule = DrawRule::Official, bool declareUnoToWin = false)
{
    std::vector<std::vector<Card>> hands;
    hands.reserve(count);
    for (std::size_t seat = 0; seat < count; ++seat) {
        const auto given = seat < special.size() ? special.at(seat) : std::vector<Card>{};
        hands.push_back(sevenCards(static_cast<std::uint32_t>(1000 * (seat + 1)), given));
    }
    return startedRound(
        {
            .seats = players(count),
            .dealer = player(count - 1),
            .deck = deckFromHands(hands, coloredCard(40, Color::Blue, Rank::Two), afterTop),
            .drawRule = drawRule,
            .declareUnoToWin = declareUnoToWin,
        },
        random);
}

[[nodiscard]] PlayCard playFive(std::uint32_t id, Color color, std::size_t targetSeat)
{
    return PlayCard{.cardId = CardId{id}, .chosenColor = color, .target = player(targetSeat)};
}

[[nodiscard]] PlayCard playPlain(std::uint32_t id)
{
    return PlayCard{.cardId = CardId{id}, .chosenColor = std::nullopt, .target = std::nullopt};
}

[[nodiscard]] std::vector<Card> greenOnes(std::uint32_t firstId, std::size_t count)
{
    std::vector<Card> cards;
    cards.reserve(count);
    for (std::uint32_t id = firstId; cards.size() < count; ++id) {
        cards.push_back(coloredCard(id, Color::Green, Rank::One));
    }
    return cards;
}

[[nodiscard]] std::size_t pendingTotal(const Round& round)
{
    const auto* awaiting = std::get_if<AwaitingStackResponse>(&round.phase());
    REQUIRE(awaiting != nullptr);
    return awaiting->total;
}

// Two players, seat 0 playing first. Both hold red Skips (ids 0.. and 100..) so that they can run down to the cards of
// interest: red Skip is playable on the red Two flipped here, and with two players it hands the turn straight back.
// Seat 0 holds `firstTail` after `skips` red Skips, seat 1 `secondTail` after its own; seat 0 is then played down to
// `firstTail` and it is its turn.
[[nodiscard]] Round twoPlayersDownTo(const std::vector<Card>& firstTail, const std::vector<Card>& secondTail,
                                     const std::vector<Card>& afterTop, SeededRandomSource& random,
                                     bool declareUnoToWin = false)
{
    const auto skips = kHandSize - firstTail.size();
    std::vector<Card> first;
    first.reserve(kHandSize);
    for (std::uint32_t id = 0; id < skips; ++id) {
        first.push_back(coloredCard(id, Color::Red, Rank::Skip));
    }
    first.insert(first.end(), firstTail.begin(), firstTail.end());
    auto second = secondTail;
    for (auto id = static_cast<std::uint32_t>(second.size()); second.size() < kHandSize; ++id) {
        second.push_back(coloredCard(100 + id, Color::Red, Rank::Five));
    }
    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckFromHands({first, second}, coloredCard(40, Color::Red, Rank::Two), afterTop),
            .declareUnoToWin = declareUnoToWin,
        },
        random);
    for (std::uint32_t id = 0; id < skips; ++id) {
        REQUIRE(round.apply(player(0), playPlain(id), random).has_value());
    }
    REQUIRE(handOf(round, player(0)).size() == firstTail.size());
    return round;
}

} // namespace

TEST_CASE("A Wild Draw Five is playable on anything", "[core][round][plusFive]")
{
    const Card top = coloredCard(1, Color::Blue, Rank::Two);
    REQUIRE(isPlayable(plusFive(0), top, Color::Blue));
    REQUIRE(isPlayable(plusFive(0), top, Color::Red));
    REQUIRE(isWild(Rank::WildDrawFive));
    REQUIRE(cardPoints(Rank::WildDrawFive) == 50);
}

TEST_CASE("Playing a Wild Draw Five sends the turn to its target, wherever they sit", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random);

    const auto events = round.apply(player(0), playFive(0, Color::Green, 2), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           ColorChosen{.player = player(0), .color = Color::Green},
                           PlusFiveTargeted{.player = player(0), .target = player(2), .total = 5},
                           TurnChanged{.player = player(2)},
                       });
    REQUIRE(round.currentPlayer() == player(2)); // chosen, not the player who sits next
    REQUIRE(round.currentColor() == Color::Green);
    REQUIRE(pendingTotal(round) == 5);
    requireRoundInvariants(round);
}

TEST_CASE("A Wild Draw Five needs a colour and a target, and the target must be another player of the round",
          "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0), coloredCard(1, Color::Blue, Rank::Five)}}, random);

    SECTION("without a target")
    {
        const auto result = round.apply(
            player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Red, .target = std::nullopt}, random);
        REQUIRE(result.error() == DomainError::TargetRequired);
    }
    SECTION("without a colour")
    {
        const auto result = round.apply(
            player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt, .target = player(1)}, random);
        REQUIRE(result.error() == DomainError::ColorRequired);
    }
    SECTION("aimed at oneself")
    {
        REQUIRE(round.apply(player(0), playFive(0, Color::Red, 0), random).error() == DomainError::InvalidTarget);
    }
    SECTION("aimed at somebody who is not in the round")
    {
        const auto result = round.apply(
            player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Red, .target = player(9)}, random);
        REQUIRE(result.error() == DomainError::InvalidTarget);
    }
    SECTION("a target given with any other card")
    {
        const auto result = round.apply(
            player(0), PlayCard{.cardId = CardId{1}, .chosenColor = std::nullopt, .target = player(1)}, random);
        REQUIRE(result.error() == DomainError::TargetNotAllowed);
    }
    // Nothing moved on a refusal
    REQUIRE(round.currentPlayer() == player(0));
    REQUIRE(handOf(round, player(0)).size() == kHandSize);
}

TEST_CASE("The target who accepts draws five and loses their turn; play resumes after them, clockwise",
          "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random, greenOnes(41, 5));
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 2), random).has_value());

    const auto events = round.apply(player(2), RespondPenalty{.response = PenaltyResponse::Accept}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           PenaltyCardsDrawn{.player = player(2),
                                             .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}, CardId{45}}},
                           PlayerSkipped{.skippedPlayer = player(2)},
                           TurnChanged{.player = player(3)},
                       });
    REQUIRE(handOf(round, player(2)).size() == kHandSize + 5);
    REQUIRE(round.currentPlayer() == player(3));
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("Play resumes after the target in the direction of play, counter-clockwise too", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    // Seat 0 plays a blue Reverse on the blue Two: from then on the turn goes 3, 2, 1, 0
    auto round =
        table({{coloredCard(0, Color::Blue, Rank::Reverse)}, {}, {}, {plusFive(300)}}, random, greenOnes(41, 5));
    REQUIRE(round.apply(player(0), playPlain(0), random).has_value());
    REQUIRE(round.currentPlayer() == player(3));
    REQUIRE(round.apply(player(3), playFive(300, Color::Green, 1), random).has_value());
    REQUIRE(round.currentPlayer() == player(1));

    REQUIRE(round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Accept}, random).has_value());

    REQUIRE(round.direction() == Direction::CounterClockwise);
    REQUIRE(round.currentPlayer() == player(0)); // after seat 1, going 3 -> 2 -> 1 -> 0
    requireRoundInvariants(round);
}

TEST_CASE("The target can answer with a Wild Draw Five: the total grows by five and the target changes",
          "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}, {}, {plusFive(200)}}, random, greenOnes(41, 10));
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 2), random).has_value());

    // Seat 2 answers and aims back at the player who aimed at them
    const auto events = round.apply(player(2), playFive(200, Color::Yellow, 0), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(2), .cardId = CardId{200}},
                           ColorChosen{.player = player(2), .color = Color::Yellow},
                           PlusFiveTargeted{.player = player(2), .target = player(0), .total = 10},
                           TurnChanged{.player = player(0)},
                       });
    REQUIRE(round.currentPlayer() == player(0));
    REQUIRE(round.currentColor() == Color::Yellow);
    REQUIRE(pendingTotal(round) == 10);
    requireRoundInvariants(round);

    // The last target accepts the whole chain; play resumes after them
    REQUIRE(round.apply(player(0), RespondPenalty{.response = PenaltyResponse::Accept}, random).has_value());
    REQUIRE(handOf(round, player(0)).size() == kHandSize - 1 + 10);
    REQUIRE(round.currentPlayer() == player(1));
}

TEST_CASE("The chain goes on as long as there is a Wild Draw Five to answer with", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}, {plusFive(100)}, {plusFive(200)}, {plusFive(300)}}, random, greenOnes(41, 20));
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());
    REQUIRE(round.apply(player(1), playFive(100, Color::Green, 2), random).has_value());
    REQUIRE(round.apply(player(2), playFive(200, Color::Green, 3), random).has_value());
    REQUIRE(round.apply(player(3), playFive(300, Color::Green, 0), random).has_value());
    REQUIRE(pendingTotal(round) == 20);

    REQUIRE(round.apply(player(0), RespondPenalty{.response = PenaltyResponse::Accept}, random).has_value());

    REQUIRE(handOf(round, player(0)).size() == kHandSize - 1 + 20);
    requireRoundInvariants(round);
}

TEST_CASE("The target of a Wild Draw Five can only answer with another Wild Draw Five", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table(
        {
            {plusFive(0)},
            {
                wildCard(100, Rank::WildDrawFour),
                coloredCard(101, Color::Green, Rank::DrawTwo),
                wildCard(102, Rank::Wild),
                coloredCard(103, Color::Green, Rank::Five),
            },
        },
        random);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());

    SECTION("not a Wild Draw Four")
    {
        const auto result = round.apply(
            player(1), PlayCard{.cardId = CardId{100}, .chosenColor = Color::Red, .target = std::nullopt}, random);
        REQUIRE(result.error() == DomainError::OnlyPlusFivePlayable);
    }
    SECTION("not a Draw Two, nor a Wild, nor a card of the colour")
    {
        for (const std::uint32_t id : {101U, 102U, 103U}) {
            CAPTURE(id);
            REQUIRE(round.apply(player(1), playPlain(id), random).error() == DomainError::OnlyPlusFivePlayable);
        }
    }
    SECTION("drawing, passing or choosing a colour are out of phase")
    {
        REQUIRE(round.apply(player(1), DrawCard{}, random).error() == DomainError::InvalidPhase);
        REQUIRE(round.apply(player(1), Pass{}, random).error() == DomainError::InvalidPhase);
        REQUIRE(round.apply(player(1), ChooseColor{.color = Color::Red}, random).error() == DomainError::InvalidPhase);
    }
    SECTION("nobody else can answer")
    {
        const auto result = round.apply(player(2), RespondPenalty{.response = PenaltyResponse::Accept}, random);
        REQUIRE(result.error() == DomainError::NotYourTurn);
    }
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(pendingTotal(round) == 5);
}

TEST_CASE("A Wild Draw Five cannot be challenged", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());

    const auto result = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random);

    REQUIRE(result.error() == DomainError::CannotChallenge);
    REQUIRE(pendingTotal(round) == 5);
}

TEST_CASE("A Wild Draw Five played as the last card ends the round: the target draws five and they count",
          "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = twoPlayersDownTo({plusFive(6)}, {}, greenOnes(41, 5), random);

    const auto events = round.apply(player(0), playFive(6, Color::Green, 1), random);

    REQUIRE(events.has_value());
    // Seven red Fives (35) and the five green Ones drawn (5): the cards drawn for the penalty count in the score
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{6}},
                           ColorChosen{.player = player(0), .color = Color::Green},
                           PlusFiveTargeted{.player = player(0), .target = player(1), .total = 5},
                           PenaltyCardsDrawn{.player = player(1),
                                             .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}, CardId{45}}},
                           RoundEnded{.winner = player(0), .points = 40},
                       });
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("Answering with the last card ends the round: the whole chain falls on the last target",
          "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    // Seat 0: two Wild Draw Five left. Seat 1 answers in the middle.
    auto round = twoPlayersDownTo({plusFive(5), plusFive(6)}, {plusFive(100)}, greenOnes(41, 15), random);
    REQUIRE(round.apply(player(0), playFive(5, Color::Green, 1), random).has_value());
    REQUIRE(round.apply(player(1), playFive(100, Color::Green, 0), random).has_value());
    REQUIRE(pendingTotal(round) == 10);

    const auto events = round.apply(player(0), playFive(6, Color::Green, 1), random);

    REQUIRE(events.has_value());
    const auto* drawn = std::get_if<PenaltyCardsDrawn>(&events->at(3));
    REQUIRE(drawn != nullptr);
    REQUIRE(drawn->player == player(1));
    REQUIRE(drawn->cards.size() == 15);
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("When the piles run short the target draws what remains, without error", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    // Nothing after the flipped card: the draw pile is empty, the discard pile holds the Two and what was played
    auto round = table({{plusFive(0)}}, random);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());

    const auto events = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Accept}, random);

    REQUIRE(events.has_value());
    REQUIRE(events->size() >= 3);
    // The discard pile is reshuffled (the top card excepted) and gives its single other card, no more
    REQUIRE(std::holds_alternative<DeckReshuffled>(events->front()));
    const auto* drawn = std::get_if<PenaltyCardsDrawn>(&events->at(1));
    REQUIRE(drawn != nullptr);
    REQUIRE(drawn->cards.size() == 1);
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("Playing a Wild Draw Five down to one card opens the UNO window, like any card", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = twoPlayersDownTo({plusFive(5), coloredCard(6, Color::Red, Rank::Five)}, {}, greenOnes(41, 10), random);

    REQUIRE(round.apply(player(0), playFive(5, Color::Green, 1), random).has_value());

    REQUIRE(round.hasUnoWindowOn(player(0)));
    requireRoundInvariants(round);
}

TEST_CASE("The target who receives cards loses the UNO window they could be caught in", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = twoPlayersDownTo({plusFive(5), coloredCard(6, Color::Red, Rank::Five)}, {plusFive(100)},
                                  greenOnes(41, 10), random);
    REQUIRE(round.apply(player(0), playFive(5, Color::Green, 1), random).has_value());
    REQUIRE(round.hasUnoWindowOn(player(0)));

    // Seat 1 answers: seat 0 is the target and receives the whole chain once they accept
    REQUIRE(round.apply(player(1), playFive(100, Color::Green, 0), random).has_value());
    REQUIRE(round.apply(player(0), RespondPenalty{.response = PenaltyResponse::Accept}, random).has_value());

    REQUIRE_FALSE(round.hasUnoWindowOn(player(0)));
    requireRoundInvariants(round);
}

TEST_CASE("With the house rule, answering with the last card needs UNO announced first", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    // Seat 1 is run down to a single Wild Draw Five by the helper's skips; 2 players, UNO obligatory
    auto round = twoPlayersDownTo({plusFive(5), plusFive(6)}, {plusFive(100)}, greenOnes(41, 20), random, true);
    REQUIRE(round.apply(player(0), playFive(5, Color::Green, 1), random).has_value());
    REQUIRE(round.mustDeclareUno(player(1)) == false); // seven cards

    // Seat 0 holds one card after this: playing it needs UNO
    REQUIRE(round.apply(player(1), playFive(100, Color::Green, 0), random).has_value());
    REQUIRE(round.mustDeclareUno(player(0)));
    REQUIRE(round.apply(player(0), playFive(6, Color::Green, 1), random).error() == DomainError::MustDeclareUno);
    REQUIRE(round.apply(player(0), CallUno{}, random).has_value());
    REQUIRE(round.apply(player(0), playFive(6, Color::Green, 1), random).has_value());
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
}

TEST_CASE("Guided draw: a target without a Wild Draw Five has no choice, with one they choose",
          "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}, {}, {plusFive(200)}}, random, greenOnes(41, 5), 4, DrawRule::Guided);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());

    // Seat 1 holds no Wild Draw Five: accepting is the only thing left, the engine says so
    REQUIRE(round.forcedAction() == std::optional<PlayerAction>{RespondPenalty{.response = PenaltyResponse::Accept}});
}

TEST_CASE("Guided draw: a target holding a Wild Draw Five is not forced", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}, {}, {plusFive(200)}}, random, greenOnes(41, 5), 4, DrawRule::Guided);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 2), random).has_value());

    REQUIRE_FALSE(round.forcedAction().has_value());
}

TEST_CASE("Official draw rule: the target always chooses", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random, greenOnes(41, 5));
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());

    REQUIRE_FALSE(round.forcedAction().has_value());
}

TEST_CASE("When the target leaves, the chain is void and play resumes after them", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 2), random).has_value());

    const auto events = round.removePlayer(player(2), random);

    REQUIRE(events.has_value());
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(round.currentPlayer() == player(3));
    requireRoundInvariants(round);
}

TEST_CASE("When somebody else leaves, the chain goes on", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random);
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 2), random).has_value());

    REQUIRE(round.removePlayer(player(3), random).has_value());

    REQUIRE(pendingTotal(round) == 5);
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("A Wild Draw Five flipped as the first card goes back into the pile", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    std::vector<std::vector<Card>> hands;
    hands.reserve(2);
    for (std::size_t seat = 0; seat < 2; ++seat) {
        hands.push_back(sevenCards(static_cast<std::uint32_t>(1000 * (seat + 1)), {}));
    }
    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckFromHands(hands, plusFive(40), {coloredCard(41, Color::Green, Rank::One)}),
        },
        random);

    REQUIRE(round.discardPile().top().rank != Rank::WildDrawFive);
    REQUIRE(round.discardPile().top().id == CardId{41});
    REQUIRE(round.drawPile().size() == 1);
}

TEST_CASE("The view of the target shows what is owed and the Wild Draw Five that can answer", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}, {plusFive(100)}}, random, greenOnes(41, 5));
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());
    const MatchProgress progress{.roundNumber = 1, .scores = {0, 0, 0, 0}, .winner = std::nullopt};

    const auto target = project(round, player(1), progress);
    const auto bystander = project(round, player(2), progress);

    REQUIRE(target.has_value());
    REQUIRE(target->phase == ViewPhase::AwaitingPenaltyResponse);
    REQUIRE(target->pendingDraw == 5);
    REQUIRE(target->currentPlayerId == player(1));
    REQUIRE(target->me.playableCardIds == std::vector<CardId>{CardId{100}});
    REQUIRE(target->me.penaltyResponse == PenaltyResponseOptions{.amount = 5, .canChallenge = false, .canStack = true});
    // The others see who is targeted and the total, nothing to answer
    REQUIRE(bystander.has_value());
    REQUIRE(bystander->pendingDraw == 5);
    REQUIRE(bystander->currentPlayerId == player(1));
    REQUIRE_FALSE(bystander->me.penaltyResponse.has_value());
    REQUIRE(bystander->me.playableCardIds.empty());
}

TEST_CASE("A target with no Wild Draw Five cannot stack", "[core][round][plusFive]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(0)}}, random, greenOnes(41, 5));
    REQUIRE(round.apply(player(0), playFive(0, Color::Green, 1), random).has_value());
    const MatchProgress progress{.roundNumber = 1, .scores = {0, 0, 0, 0}, .winner = std::nullopt};

    const auto target = project(round, player(1), progress);

    REQUIRE(target.has_value());
    REQUIRE(target->me.penaltyResponse->canStack == false);
    REQUIRE(target->me.playableCardIds.empty());
}
