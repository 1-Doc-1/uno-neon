#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/legal_actions.hpp"
#include "uno/testing/seeded_random_source.hpp"
#include "uno/testing/view_leak_check.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

using uno::core::CallUno;
using uno::core::Card;
using uno::core::CardId;
using uno::core::Color;
using uno::core::Direction;
using uno::core::DomainError;
using uno::core::DrawCard;
using uno::core::Match;
using uno::core::MatchLength;
using uno::core::MatchProgress;
using uno::core::MyState;
using uno::core::PenaltyResponseOptions;
using uno::core::PlayCard;
using uno::core::PlayerView;
using uno::core::project;
using uno::core::Rank;
using uno::core::Round;
using uno::core::RoundOver;
using uno::core::RoundResult;
using uno::core::SeatView;
using uno::core::ViewPhase;
using uno::testing::coloredCard;
using uno::testing::deckFromHands;
using uno::testing::legalActionsOfCurrentPlayer;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireViewLeaksNothing;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

namespace {

constexpr std::uint64_t kSeed = 42;
constexpr std::size_t kStepLimit = 20'000;
constexpr std::uint32_t kFillerFirstId = 100;

[[nodiscard]] MatchProgress progressFor(std::size_t playerCount)
{
    return {
        .roundNumber = 1,
        .scores = std::vector<std::uint32_t>(playerCount, 0),
        .winner = std::nullopt,
    };
}

[[nodiscard]] PlayerView viewOf(const Round& round, std::size_t viewer, std::size_t playerCount)
{
    const auto view = project(round, player(viewer), progressFor(playerCount));
    REQUIRE(view.has_value());
    return *view;
}

// Seven cards of one color and rank, ids starting at `firstId`.
[[nodiscard]] std::vector<Card> sevenOf(std::uint32_t firstId, Color color, Rank rank)
{
    std::vector<Card> hand;
    hand.reserve(7);
    for (std::uint32_t id = firstId; id < firstId + 7; ++id) {
        hand.push_back(coloredCard(id, color, rank));
    }
    return hand;
}

// The red Fives held by a seat after the first one (ids 107-113 for seat 1, 114-120 for seat 2...).
[[nodiscard]] std::vector<Card> fillerHand(std::uint32_t seat)
{
    return sevenOf(kFillerFirstId + (seat * 7), Color::Red, Rank::Five);
}

// Seven varied cards no fixture uses elsewhere, ids starting at `firstId`.
[[nodiscard]] std::vector<Card> variedHand(std::uint32_t firstId)
{
    return {
        coloredCard(firstId, Color::Yellow, Rank::Nine),
        coloredCard(firstId + 1, Color::Green, Rank::Skip),
        wildCard(firstId + 2, Rank::Wild),
        coloredCard(firstId + 3, Color::Blue, Rank::DrawTwo),
        coloredCard(firstId + 4, Color::Yellow, Rank::Seven),
        wildCard(firstId + 5, Rank::WildDrawFour),
        coloredCard(firstId + 6, Color::Green, Rank::Zero),
    };
}

[[nodiscard]] std::vector<Card> cardsOf(std::uint32_t firstId, Color color, std::size_t count)
{
    std::vector<Card> cards;
    cards.reserve(count);
    for (std::size_t offset = 0; offset < count; ++offset) {
        cards.push_back(coloredCard(firstId + static_cast<std::uint32_t>(offset), color, Rank::Two));
    }
    return cards;
}

// One hand per seat, in seat order (the last seat deals, so seat 0 plays first).
[[nodiscard]] Round dealtRound(const std::vector<std::vector<Card>>& hands, const Card& top,
                               const std::vector<Card>& afterTop, SeededRandomSource& random)
{
    return startedRound(
        {
            .seats = players(hands.size()),
            .dealer = player(hands.size() - 1),
            .deck = deckFromHands(hands, top, afterTop),
        },
        random);
}

// Seat 0 holds `firstHand`, every other seat holds red Fives (fillerHand).
[[nodiscard]] Round dealtRoundWithFillers(std::size_t playerCount, const std::vector<Card>& firstHand, const Card& top,
                                          const std::vector<Card>& afterTop, SeededRandomSource& random)
{
    std::vector<std::vector<Card>> hands{firstHand};
    for (std::uint32_t seat = 1; seat < playerCount; ++seat) {
        hands.push_back(fillerHand(seat));
    }
    return dealtRound(hands, top, afterTop, random);
}

void play(Round& round, SeededRandomSource& random, std::uint32_t cardId, std::optional<Color> color = std::nullopt)
{
    REQUIRE(round.apply(round.currentPlayer(), PlayCard{.cardId = CardId{cardId}, .chosenColor = color}, random)
                .has_value());
}

// Two players; red Two is flipped. player-0 holds six red Skips (ids 0-5, each hands the turn straight
// back with two players) and a red Five (id 6); player-1 holds red Fives.
[[nodiscard]] Round roundWithSkipsAndAFive(SeededRandomSource& random)
{
    auto hand = sevenOf(0, Color::Red, Rank::Skip);
    hand.back() = coloredCard(6, Color::Red, Rank::Five);
    return dealtRoundWithFillers(2, hand, coloredCard(40, Color::Red, Rank::Two), {}, random);
}

void playToTheEnd(Match& match, SeededRandomSource& random)
{
    for (std::size_t step = 0; step < kStepLimit && !match.winner().has_value(); ++step) {
        const auto actions = legalActionsOfCurrentPlayer(match.round());
        REQUIRE(match.apply(match.round().currentPlayer(), actions.front(), random).has_value());
    }
}

// Every score is 0 but the winner's, which is the points of the round.
[[nodiscard]] std::vector<std::uint32_t> onlyTheWinnerScores(const RoundOver& over, std::size_t playerCount)
{
    std::vector<std::uint32_t> scores;
    scores.reserve(playerCount);
    for (std::size_t seat = 0; seat < playerCount; ++seat) {
        scores.push_back(player(seat) == over.winner ? over.points : 0U);
    }
    return scores;
}

[[nodiscard]] std::vector<std::uint32_t> scoresOf(const PlayerView& view)
{
    std::vector<std::uint32_t> scores;
    scores.reserve(view.players.size());
    for (const auto& seat : view.players) {
        scores.push_back(seat.score);
    }
    return scores;
}

void requireEveryViewLeaksNothing(const Match& match, std::size_t playerCount)
{
    for (std::size_t viewer = 0; viewer < playerCount; ++viewer) {
        const auto view = project(match, player(viewer));
        REQUIRE(view.has_value());
        requireViewLeaksNothing(match.round(), *view, player(viewer));
    }
}

void playRandomMatchCheckingEveryView(std::uint64_t seed)
{
    SeededRandomSource random{seed};
    auto started = Match::start(players(4), {.matchLength = MatchLength::SingleRound}, random);
    REQUIRE(started.has_value());
    auto match = std::move(started->match);

    for (std::size_t step = 0; step < kStepLimit && !match.winner().has_value(); ++step) {
        const auto actions = legalActionsOfCurrentPlayer(match.round());
        const auto& chosen = actions.at(random.uniform(static_cast<std::uint32_t>(actions.size())));
        REQUIRE(match.apply(match.round().currentPlayer(), chosen, random).has_value());
        requireEveryViewLeaksNothing(match, 4);
    }
    REQUIRE(match.winner().has_value());
}

} // namespace

TEST_CASE("The view of the current player lists their hand and what they may do", "[core][view]")
{
    SeededRandomSource random{kSeed};
    // Only the red Five matches the red Two on top.
    auto hand = sevenOf(0, Color::Green, Rank::Four);
    hand.at(0) = coloredCard(0, Color::Red, Rank::Five);
    hand.at(1) = coloredCard(1, Color::Blue, Rank::Seven);
    const auto top = coloredCard(40, Color::Red, Rank::Two);
    const auto round = dealtRoundWithFillers(3, hand, top, cardsOf(50, Color::Blue, 3), random);

    const auto view = viewOf(round, 0, 3);

    REQUIRE(view.me == MyState{
                           .playerId = player(0),
                           .hand = hand,
                           .playableCardIds = {CardId{0}},
                           .canDraw = true,
                           .canKeepDrawnCard = false,
                           .canCallUno = false,
                           .canChooseColor = false,
                           .penaltyResponse = std::nullopt,
                       });
    REQUIRE(view.players == std::vector<SeatView>{
                                {.playerId = player(0), .seat = 0, .cardCount = 7, .score = 0, .hasCalledUno = false},
                                {.playerId = player(1), .seat = 1, .cardCount = 7, .score = 0, .hasCalledUno = false},
                                {.playerId = player(2), .seat = 2, .cardCount = 7, .score = 0, .hasCalledUno = false},
                            });
    requireViewLeaksNothing(round, view, player(0));
}

TEST_CASE("The view of a fresh round shows the public table", "[core][view]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Red, Rank::Two);
    const auto round =
        dealtRoundWithFillers(3, sevenOf(0, Color::Green, Rank::Four), top, cardsOf(50, Color::Blue, 3), random);

    const auto view = viewOf(round, 0, 3);

    REQUIRE(view.phase == ViewPhase::AwaitingPlay);
    REQUIRE(view.currentPlayerId == player(0));
    REQUIRE(view.direction == Direction::Clockwise);
    REQUIRE(view.currentColor == Color::Red);
    REQUIRE(view.discardTop == top);
    REQUIRE(view.drawPileCount == 3);
    REQUIRE(view.pendingDraw == 0);
    REQUIRE(view.round == 1);
    REQUIRE(view.roundResult == std::nullopt);
    REQUIRE(view.matchWinnerId == std::nullopt);
}

TEST_CASE("A player waiting for their turn sees their hand but cannot act", "[core][view]")
{
    SeededRandomSource random{kSeed};
    const auto round = dealtRoundWithFillers(3, sevenOf(0, Color::Red, Rank::Five),
                                             coloredCard(40, Color::Red, Rank::Two), {}, random);

    const auto view = viewOf(round, 1, 3);

    REQUIRE(view.me.playerId == player(1));
    REQUIRE(view.me.hand == fillerHand(1));
    REQUIRE(view.me.playableCardIds.empty());
    REQUIRE(!view.me.canDraw);
    REQUIRE(!view.me.canKeepDrawnCard);
    REQUIRE(view.currentPlayerId == player(0));
    requireViewLeaksNothing(round, view, player(1));
}

TEST_CASE("After drawing a playable card, only that card can be played, or the turn passed", "[core][view]")
{
    SeededRandomSource random{kSeed};
    // player-0 holds no red card; the red Three they draw is playable.
    auto round = dealtRoundWithFillers(2, sevenOf(0, Color::Blue, Rank::Four), coloredCard(40, Color::Red, Rank::Two),
                                       {coloredCard(41, Color::Red, Rank::Three)}, random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    const auto view = viewOf(round, 0, 2);

    REQUIRE(view.phase == ViewPhase::AwaitingDrawnCardDecision);
    REQUIRE(view.me.playableCardIds == std::vector<CardId>{CardId{41}});
    REQUIRE(!view.me.canDraw);
    REQUIRE(view.me.canKeepDrawnCard);
}

TEST_CASE("A flipped Wild leaves the color to choose, and the view says so", "[core][view]")
{
    SeededRandomSource random{kSeed};
    const auto round =
        dealtRoundWithFillers(2, sevenOf(0, Color::Blue, Rank::Four), wildCard(40, Rank::Wild), {}, random);

    const auto chooser = viewOf(round, 0, 2);
    const auto other = viewOf(round, 1, 2);

    REQUIRE(chooser.phase == ViewPhase::AwaitingColorChoice);
    REQUIRE(chooser.currentColor == std::nullopt);
    REQUIRE(chooser.me.canChooseColor);
    REQUIRE(!chooser.me.canDraw);
    REQUIRE(chooser.me.playableCardIds.empty());
    REQUIRE(!other.me.canChooseColor);
}

TEST_CASE("A pending Wild Draw Four offers its target to accept or challenge", "[core][view]")
{
    SeededRandomSource random{kSeed};
    auto hand = sevenOf(0, Color::Red, Rank::Five);
    hand.front() = wildCard(0, Rank::WildDrawFour);
    auto round = dealtRoundWithFillers(3, hand, coloredCard(40, Color::Blue, Rank::Two), {}, random);
    play(round, random, 0, Color::Green);

    const auto target = viewOf(round, 1, 3);
    const auto poser = viewOf(round, 0, 3);

    REQUIRE(target.phase == ViewPhase::AwaitingPenaltyResponse);
    REQUIRE(target.pendingDraw == 4);
    REQUIRE(target.me.penaltyResponse == PenaltyResponseOptions{.amount = 4, .canChallenge = true, .canStack = false});
    REQUIRE(target.me.playableCardIds.empty());
    REQUIRE(!target.me.canDraw);
    REQUIRE(poser.pendingDraw == 4);
    REQUIRE(poser.me.penaltyResponse == std::nullopt);
}

TEST_CASE("A player about to play their second-to-last card may announce UNO", "[core][view]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithSkipsAndAFive(random);
    for (std::uint32_t id = 0; id < 5; ++id) {
        play(round, random, id);
    }

    REQUIRE(viewOf(round, 0, 2).me.canCallUno);
    REQUIRE(!viewOf(round, 1, 2).me.canCallUno);
}

TEST_CASE("The call button is offered to the offender, and only to them", "[core][view]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithSkipsAndAFive(random);
    for (std::uint32_t id = 0; id < 5; ++id) {
        play(round, random, id);
    }
    play(round, random, 6);

    const auto offender = viewOf(round, 0, 2);
    const auto other = viewOf(round, 1, 2);
    REQUIRE(offender.me.canCallUno);
    REQUIRE(round.hasUnoWindowOn(player(0)));
    REQUIRE(!other.me.canCallUno);
}

TEST_CASE("Once the offender announced UNO, nobody can be caught", "[core][view]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithSkipsAndAFive(random);
    for (std::uint32_t id = 0; id < 5; ++id) {
        play(round, random, id);
    }
    play(round, random, 6);

    REQUIRE(round.apply(player(0), CallUno{}, random).has_value());

    const auto announced = viewOf(round, 1, 2);
    REQUIRE(round.unoWindows().empty());
    REQUIRE(announced.players.at(0).hasCalledUno);
    REQUIRE(!announced.players.at(1).hasCalledUno);
}

TEST_CASE("A finished round reveals the remaining hands to everybody", "[core][view]")
{
    SeededRandomSource random{kSeed};
    auto round = dealtRoundWithFillers(2, sevenOf(0, Color::Red, Rank::Skip), coloredCard(40, Color::Red, Rank::Two),
                                       {}, random);
    for (std::uint32_t id = 0; id < 7; ++id) {
        play(round, random, id);
    }

    const auto winner = viewOf(round, 0, 2);
    const auto loser = viewOf(round, 1, 2);

    REQUIRE(loser.phase == ViewPhase::RoundOver);
    REQUIRE(loser.roundResult == RoundResult{
                                     .winnerId = player(0),
                                     .points = 35,
                                     .revealedHands = {{.playerId = player(0), .cards = {}},
                                                       {.playerId = player(1), .cards = fillerHand(1)}},
                                 });
    REQUIRE(winner.roundResult == loser.roundResult);
    REQUIRE(!loser.me.canDraw);
    requireViewLeaksNothing(round, loser, player(1));
}

TEST_CASE("The view of a match tells its scores and, once over, its winner", "[core][view]")
{
    SeededRandomSource random{kSeed};
    auto started = Match::start(players(3), {.matchLength = MatchLength::SingleRound}, random);
    REQUIRE(started.has_value());
    auto match = std::move(started->match);
    playToTheEnd(match, random);
    const auto* over = std::get_if<RoundOver>(&match.round().phase());
    REQUIRE(over != nullptr);

    const auto view = project(match, player(2));

    REQUIRE(view.has_value());
    REQUIRE(view->phase == ViewPhase::MatchOver);
    REQUIRE(view->matchWinnerId == over->winner);
    REQUIRE(view->round == 1);
    REQUIRE(scoresOf(*view) == onlyTheWinnerScores(*over, 3));
}

TEST_CASE("Projecting for a player who is not seated is rejected", "[core][view]")
{
    SeededRandomSource random{kSeed};
    const auto round = dealtRoundWithFillers(2, sevenOf(0, Color::Red, Rank::Five),
                                             coloredCard(40, Color::Red, Rank::Two), {}, random);

    REQUIRE(project(round, player(9), progressFor(2)).error() == DomainError::UnknownPlayer);
}

// Anti-leak, by non-interference: two rounds that differ only in what a viewer must not know give
// that viewer exactly the same view. A field that leaked hidden information would break the equality.

TEST_CASE("A view does not depend on the other hands nor on the draw pile", "[core][view][leak]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Red, Rank::Two);
    const auto sharedHand = variedHand(200);
    const auto roundA =
        dealtRound({sevenOf(100, Color::Red, Rank::Five), sevenOf(110, Color::Red, Rank::Five), sharedHand}, top,
                   cardsOf(300, Color::Green, 5), random);
    const auto roundB =
        dealtRound({variedHand(400), variedHand(410), sharedHand}, top, cardsOf(500, Color::Yellow, 5), random);
    REQUIRE(!(roundA == roundB));

    // player-2 knows their own hand only: nothing else may show.
    REQUIRE(viewOf(roundA, 2, 3) == viewOf(roundB, 2, 3));
}

TEST_CASE("The current player's view does not depend on the others' cards either", "[core][view][leak]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Red, Rank::Two);
    const auto sharedHand = variedHand(200);
    const auto roundA =
        dealtRound({sharedHand, sevenOf(100, Color::Red, Rank::Five)}, top, cardsOf(300, Color::Green, 4), random);
    const auto roundB = dealtRound({sharedHand, variedHand(400)}, top, cardsOf(500, Color::Blue, 4), random);

    REQUIRE(viewOf(roundA, 0, 2) == viewOf(roundB, 0, 2));
}

TEST_CASE("The target of a Wild Draw Four cannot tell a legal one from a bluff", "[core][view][leak]")
{
    SeededRandomSource random{kSeed};
    // Blue is current. In A, player-0 holds no blue card (the +4 is legal); in B, they keep one (a bluff).
    auto legalHand = sevenOf(0, Color::Red, Rank::Five);
    legalHand.front() = wildCard(0, Rank::WildDrawFour);
    auto bluffHand = legalHand;
    bluffHand.at(1) = coloredCard(1, Color::Blue, Rank::Six);
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto roundA = dealtRound({legalHand, fillerHand(1), fillerHand(2)}, top, {}, random);
    auto roundB = dealtRound({bluffHand, fillerHand(1), fillerHand(2)}, top, {}, random);
    play(roundA, random, 0, Color::Green);
    play(roundB, random, 0, Color::Green);

    REQUIRE(viewOf(roundA, 1, 3) == viewOf(roundB, 1, 3));
    REQUIRE(viewOf(roundA, 2, 3) == viewOf(roundB, 2, 3));
}

TEST_CASE("No view of a random match ever shows a card its viewer may not see", "[core][view][leak]")
{
    playRandomMatchCheckingEveryView(1);
    playRandomMatchCheckingEveryView(2);
    playRandomMatchCheckingEveryView(3);
}
