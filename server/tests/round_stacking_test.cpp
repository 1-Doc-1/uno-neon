#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
#include "uno/core/penalty_stacking.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

// Penalty stacking with the ladder (ADR 0029): a +2, a +4 or a +5 goes on a pending penalty of a lower or equal level,
// whatever the colors, and the amounts add up. Nobody gets a window to answer, and a +4 cannot be challenged.

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

constexpr std::uint64_t kSeed = 7;

[[nodiscard]] Card plusTwo(std::uint32_t id, Color color = Color::Blue)
{
    return coloredCard(id, color, Rank::DrawTwo);
}
[[nodiscard]] Card plusFour(std::uint32_t id)
{
    return wildCard(id, Rank::WildDrawFour);
}
[[nodiscard]] Card plusFive(std::uint32_t id)
{
    return wildCard(id, Rank::WildDrawFive);
}

[[nodiscard]] std::vector<Card> sevenCards(std::uint32_t firstId, const std::vector<Card>& special)
{
    std::vector<Card> hand = special;
    for (auto slot = static_cast<std::uint32_t>(special.size()); slot < kHandSize; ++slot) {
        hand.push_back(coloredCard(firstId + slot, Color::Red, Rank::Five));
    }
    return hand;
}

// `special` lists, seat by seat, the cards each hand starts with (the rest is red Fives). Seat 0 plays first on a blue
// Two; `afterTop` follows in the draw pile.
[[nodiscard]] Round table(const std::vector<std::vector<Card>>& special, SeededRandomSource& random,
                          const std::vector<Card>& afterTop = {}, std::size_t count = 3,
                          DrawRule drawRule = DrawRule::Official, PenaltyStacking stacking = PenaltyStacking::Ladder)
{
    std::vector<std::vector<Card>> hands;
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
            .stacking = stacking,
        },
        random);
}

[[nodiscard]] std::vector<Card> redFives(std::uint32_t firstId, std::size_t count)
{
    std::vector<Card> cards;
    for (std::uint32_t id = firstId; cards.size() < count; ++id) {
        cards.push_back(coloredCard(id, Color::Red, Rank::Five));
    }
    return cards;
}

[[nodiscard]] PlayCard playCard(std::uint32_t id, std::optional<Color> color = std::nullopt,
                                std::optional<std::size_t> targetSeat = std::nullopt)
{
    return PlayCard{
        .cardId = CardId{id},
        .chosenColor = color,
        .target = targetSeat.has_value() ? std::optional<PlayerId>{player(*targetSeat)} : std::nullopt,
    };
}

// Two players on a red Two, seat 0 to play. Seat 1 holds `tail` after playing out its red Skips (each hands the turn
// straight back with two players) and one red Five that ends its turn. Seat 0 plays a red Five, waits for seat 1 to
// play down, then plays a red Draw Two (id 2): the state returned is seat 1 facing it, holding exactly `tail`.
[[nodiscard]] Round twoPlayersFacingPlusTwo(const std::vector<Card>& tail, SeededRandomSource& random,
                                            const std::vector<Card>& afterTop = {}, bool declareUnoToWin = false)
{
    const auto skips = kHandSize - 1 - tail.size();
    std::vector<Card> first{coloredCard(1, Color::Red, Rank::Five), coloredCard(2, Color::Red, Rank::DrawTwo)};
    std::ranges::copy(redFives(3, kHandSize - 2), std::back_inserter(first));
    std::vector<Card> second;
    second.reserve(kHandSize);
    for (std::uint32_t id = 0; id < skips; ++id) {
        second.push_back(coloredCard(100 + id, Color::Red, Rank::Skip));
    }
    second.push_back(coloredCard(150, Color::Red, Rank::Five));
    second.insert(second.end(), tail.begin(), tail.end());

    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckFromHands({first, second}, coloredCard(40, Color::Red, Rank::Two), afterTop),
            .declareUnoToWin = declareUnoToWin,
            .stacking = PenaltyStacking::Ladder,
        },
        random);
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());
    for (std::uint32_t id = 0; id < skips; ++id) {
        REQUIRE(round.apply(player(1), playCard(100 + id), random).has_value());
    }
    REQUIRE(round.apply(player(1), playCard(150), random).has_value());
    REQUIRE(round.apply(player(0), playCard(2), random).has_value());
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(handOf(round, player(1)).size() == tail.size());
    return round;
}

// Plays the penalty card `id` of rank `rank`, with the color and the target it needs.
[[nodiscard]] PlayCard playPenalty(std::uint32_t id, Rank rank, std::size_t targetSeat)
{
    switch (rank) {
    case Rank::WildDrawFive:
        return playCard(id, Color::Green, targetSeat);
    case Rank::WildDrawFour:
        return playCard(id, Color::Green);
    default:
        return playCard(id);
    }
}

[[nodiscard]] const AwaitingStackResponse& pending(const Round& round)
{
    const auto* awaiting = std::get_if<AwaitingStackResponse>(&round.phase());
    REQUIRE(awaiting != nullptr);
    return *awaiting;
}

constexpr RespondPenalty kAccept{.response = PenaltyResponse::Accept};
constexpr RespondPenalty kChallenge{.response = PenaltyResponse::Challenge};

} // namespace

TEST_CASE("With the ladder, a Draw Two sends the turn to the next player, who owes the cards but has not drawn them",
          "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}}, random);

    const auto events = round.apply(player(0), playCard(1), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{1}},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(pending(round).total == 2);
    REQUIRE(pending(round).top == Rank::DrawTwo);
    REQUIRE(handOf(round, player(1)).size() == kHandSize);
    requireRoundInvariants(round);
}

TEST_CASE("Without the ladder, a Draw Two is still drawn at once", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}}, random, redFives(500, 4), 3, DrawRule::Official, PenaltyStacking::Official);

    REQUIRE(round.apply(player(0), playCard(1), random).has_value());

    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 2);
    REQUIRE(round.currentPlayer() == player(2));
}

TEST_CASE("The ladder: a penalty card goes on a pending penalty of a lower or equal level, and only then",
          "[core][round][stacking]")
{
    const auto [pendingRank, pendingId] =
        GENERATE(std::pair{Rank::DrawTwo, 1U}, std::pair{Rank::WildDrawFour, 2U}, std::pair{Rank::WildDrawFive, 3U});
    const auto [answerRank, answerId] =
        GENERATE(std::pair{Rank::DrawTwo, 11U}, std::pair{Rank::WildDrawFour, 12U}, std::pair{Rank::WildDrawFive, 13U});
    CAPTURE(pendingRank, answerRank);

    SeededRandomSource random{kSeed};
    auto round = table(
        {{plusTwo(1), plusFour(2), plusFive(3)}, {plusTwo(11, Color::Green), plusFour(12), plusFive(13)}}, random);
    REQUIRE(round.apply(player(0), playPenalty(pendingId, pendingRank, 1), random).has_value());
    REQUIRE(round.currentPlayer() == player(1));
    const auto owed = pending(round).total;

    const auto result = round.apply(player(1), playPenalty(answerId, answerRank, 2), random);

    if (canStackOn(answerRank, pendingRank)) {
        REQUIRE(result.has_value());
        REQUIRE(pending(round).total == owed + penaltyCards(answerRank));
        REQUIRE(pending(round).top == answerRank);
        // A Draw Two and a Wild Draw Four go to the next player, a Wild Draw Five to the chosen target: seat 2 here.
        REQUIRE(round.currentPlayer() == player(2));
        requireRoundInvariants(round);
    } else {
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() ==
                (pendingRank == Rank::WildDrawFive ? DomainError::OnlyPlusFivePlayable : DomainError::NotStackable));
        REQUIRE(round.currentPlayer() == player(1)); // nothing moved
        REQUIRE(pending(round).total == owed);
    }
}

TEST_CASE("The ladder compares levels, not colors", "[core][round][stacking]")
{
    REQUIRE(canStackOn(Rank::DrawTwo, Rank::DrawTwo));
    REQUIRE(canStackOn(Rank::WildDrawFour, Rank::DrawTwo));
    REQUIRE(canStackOn(Rank::WildDrawFive, Rank::DrawTwo));
    REQUIRE(canStackOn(Rank::WildDrawFour, Rank::WildDrawFour));
    REQUIRE(canStackOn(Rank::WildDrawFive, Rank::WildDrawFour));
    REQUIRE(canStackOn(Rank::WildDrawFive, Rank::WildDrawFive));
    REQUIRE_FALSE(canStackOn(Rank::DrawTwo, Rank::WildDrawFour));
    REQUIRE_FALSE(canStackOn(Rank::DrawTwo, Rank::WildDrawFive));
    REQUIRE_FALSE(canStackOn(Rank::WildDrawFour, Rank::WildDrawFive));
    REQUIRE_FALSE(canStackOn(Rank::Five, Rank::DrawTwo));
    REQUIRE_FALSE(canStackOn(Rank::Wild, Rank::DrawTwo));
}

TEST_CASE("A green Draw Two goes on a blue one", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}, {plusTwo(11, Color::Green)}}, random);
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());

    REQUIRE(round.apply(player(1), playCard(11), random).has_value());

    REQUIRE(pending(round).total == 4);
    REQUIRE(round.currentColor() == Color::Green);
}

TEST_CASE("A card that is not a penalty card cannot go on a pending penalty", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}, {coloredCard(11, Color::Blue, Rank::Two), wildCard(12, Rank::Wild)}}, random);
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());

    REQUIRE(round.apply(player(1), playCard(11), random).error() == DomainError::NotStackable);
    REQUIRE(round.apply(player(1), playCard(12, Color::Red), random).error() == DomainError::NotStackable);
}

TEST_CASE("Three players: +2, then +4, then +5 add up, and the last target draws everything and loses the turn",
          "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}, {plusFour(12)}, {plusFive(23)}}, random, redFives(500, 20));

    REQUIRE(round.apply(player(0), playCard(1), random).has_value());
    REQUIRE(round.apply(player(1), playCard(12, Color::Red), random).has_value());
    REQUIRE(pending(round).total == 6);
    REQUIRE(round.currentPlayer() == player(2));
    REQUIRE(round.apply(player(2), playCard(23, Color::Yellow, 0), random).has_value());
    REQUIRE(pending(round).total == 11);
    REQUIRE(round.currentPlayer() == player(0)); // the +5 was aimed at the first player
    requireRoundInvariants(round);

    const auto events = round.apply(player(0), kAccept, random);

    REQUIRE(events.has_value());
    REQUIRE(handOf(round, player(0)).size() == kHandSize - 1 + 11);
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(round.currentPlayer() == player(1)); // seat 0 lost the turn
    requireRoundInvariants(round);
}

TEST_CASE("With the ladder, a click on the draw pile takes the whole penalty", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}, {plusTwo(11, Color::Green)}}, random, redFives(500, 6));
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());
    REQUIRE(round.canDraw(player(1)));

    const auto events = round.apply(player(1), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 2);
    REQUIRE(round.currentPlayer() == player(2));
}

TEST_CASE("Only the ladder lets a click on the pile take a penalty", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFive(3)}}, random, {}, 3, DrawRule::Official, PenaltyStacking::Official);
    REQUIRE(round.apply(player(0), playCard(3, Color::Red, 1), random).has_value());

    REQUIRE_FALSE(round.canDraw(player(1)));
    REQUIRE(round.apply(player(1), DrawCard{}, random).error() == DomainError::InvalidPhase);
}

TEST_CASE("With the ladder, a Wild Draw Four cannot be challenged, whether or not it was a bluff",
          "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    // Seat 0 holds a blue card: playing the +4 on blue is a bluff in the official rules.
    auto round = table({{plusFour(2), coloredCard(3, Color::Blue, Rank::Five)}}, random);

    REQUIRE(round.apply(player(0), playCard(2, Color::Red), random).has_value());

    REQUIRE(round.apply(player(1), kChallenge, random).error() == DomainError::CannotChallenge);
    REQUIRE(pending(round).total == 4);
}

TEST_CASE("With the official rules, a Wild Draw Four is still challenged", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFour(2), coloredCard(3, Color::Blue, Rank::Five)}}, random, redFives(500, 8), 3,
                       DrawRule::Official, PenaltyStacking::Official);

    REQUIRE(round.apply(player(0), playCard(2, Color::Red), random).has_value());

    REQUIRE(std::holds_alternative<AwaitingPenaltyResponse>(round.phase()));
    REQUIRE(round.apply(player(1), kChallenge, random).has_value());
}

TEST_CASE("The target sees which of their cards can go on the pending penalty, from the moment it arrives",
          "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusFour(2)}, {plusTwo(11, Color::Green), plusFour(12), plusFive(13)}}, random);
    REQUIRE(round.apply(player(0), playCard(2, Color::Red), random).has_value());

    const MatchProgress progress{.roundNumber = 1, .scores = std::vector<std::uint32_t>(3, 0), .winner = std::nullopt};
    const auto view = project(round, player(1), progress);

    REQUIRE(view.has_value());
    REQUIRE(view->pendingDraw == 4);
    REQUIRE(view->me.playableCardIds == std::vector<CardId>{CardId{12}, CardId{13}}); // not the +2: it is too weak
    REQUIRE(view->me.penaltyResponse.has_value());
    REQUIRE(view->me.penaltyResponse->canStack);
    REQUIRE_FALSE(view->me.penaltyResponse->canChallenge);
    REQUIRE(view->me.canDraw);
}

TEST_CASE("Guided draw: a target with nothing to stack draws by itself, a target with a card chooses",
          "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    SECTION("nothing to stack")
    {
        auto round = table({{plusFive(3)}}, random, {}, 3, DrawRule::Guided);
        REQUIRE(round.apply(player(0), playCard(3, Color::Red, 1), random).has_value());
        REQUIRE(round.forcedAction() == PlayerAction{kAccept});
    }
    SECTION("a card that is high enough")
    {
        auto round = table({{plusFour(2)}, {plusFour(12)}}, random, {}, 3, DrawRule::Guided);
        REQUIRE(round.apply(player(0), playCard(2, Color::Red), random).has_value());
        REQUIRE_FALSE(round.forcedAction().has_value());
    }
    SECTION("only cards that are too low")
    {
        auto round = table({{plusFour(2)}, {plusTwo(11, Color::Green)}}, random, {}, 3, DrawRule::Guided);
        REQUIRE(round.apply(player(0), playCard(2, Color::Red), random).has_value());
        REQUIRE(round.forcedAction() == PlayerAction{kAccept});
    }
}

TEST_CASE("The direction decides who the Draw Two is aimed at", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{coloredCard(1, Color::Blue, Rank::Reverse)}, {}, {plusTwo(21, Color::Blue)}}, random);

    REQUIRE(round.apply(player(0), playCard(1), random).has_value());
    REQUIRE(round.currentPlayer() == player(2)); // reversed: the previous seat plays
    REQUIRE(round.apply(player(2), playCard(21), random).has_value());

    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(pending(round).total == 2);
}

TEST_CASE("When the piles run dry, the target draws what is left of the penalty", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}, {plusTwo(11, Color::Green)}}, random); // nothing after the opening card
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());
    REQUIRE(round.apply(player(1), playCard(11), random).has_value()); // 4 owed by seat 2
    REQUIRE(round.drawPile().cards().empty());

    const auto events = round.apply(player(2), kAccept, random);

    REQUIRE(events.has_value());
    // The discard pile holds the opening Two and the two Draw Twos: the Two and the first Draw Two go back into the
    // pile, which is all there is to draw of the four owed.
    REQUIRE(handOf(round, player(2)).size() == kHandSize + 2);
    requireRoundInvariants(round);
}

TEST_CASE("Stacking down to one card opens the UNO window on the player who stacked", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = twoPlayersFacingPlusTwo({plusTwo(60, Color::Green), coloredCard(61, Color::Red, Rank::Five)}, random,
                                         redFives(500, 8));

    REQUIRE(round.apply(player(1), playCard(60), random).has_value());

    REQUIRE(pending(round).total == 4);
    REQUIRE(round.currentPlayer() == player(0));
    REQUIRE(round.hasUnoWindowOn(player(1)));
    requireRoundInvariants(round);
}

TEST_CASE("Stacking the last card ends the round and the last target draws the whole stack", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = twoPlayersFacingPlusTwo({plusTwo(60, Color::Green)}, random, redFives(500, 8));

    const auto events = round.apply(player(1), playCard(60), random);

    REQUIRE(events.has_value());
    const auto* over = std::get_if<RoundOver>(&round.phase());
    REQUIRE(over != nullptr);
    REQUIRE(over->winner == player(1));
    // Seat 0 played two of its seven cards and draws the 4 owed: 5 + 4 red Fives left, 5 points each.
    REQUIRE(handOf(round, player(0)).size() == 9);
    REQUIRE(over->points == 45);
    requireRoundInvariants(round);
}

TEST_CASE("With the UNO house rule, the last card cannot be stacked before UNO is announced", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = twoPlayersFacingPlusTwo({plusTwo(60, Color::Green)}, random, redFives(500, 8), true);

    REQUIRE(round.mustDeclareUno(player(1)));
    REQUIRE(round.apply(player(1), playCard(60), random).error() == DomainError::MustDeclareUno);

    REQUIRE(round.apply(player(1), CallUno{}, random).has_value());
    REQUIRE_FALSE(round.mustDeclareUno(player(1)));
    REQUIRE(round.apply(player(1), playCard(60), random).has_value());
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
}

TEST_CASE("A player who leaves while a penalty is pending for them hands the turn on", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}}, random);
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());

    const auto events = round.removePlayer(player(1), random);

    REQUIRE(events.has_value());
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("A player who leaves while someone else owes a stack does not cancel it", "[core][round][stacking]")
{
    SeededRandomSource random{kSeed};
    auto round = table({{plusTwo(1)}}, random, {}, 4);
    REQUIRE(round.apply(player(0), playCard(1), random).has_value());

    REQUIRE(round.removePlayer(player(3), random).has_value());

    REQUIRE(pending(round).total == 2);
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}
