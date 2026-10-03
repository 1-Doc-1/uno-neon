#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

using uno::core::CallUno;
using uno::core::Card;
using uno::core::CardId;
using uno::core::CardPlayed;
using uno::core::CatchUno;
using uno::core::ChooseColor;
using uno::core::Color;
using uno::core::ColorChosen;
using uno::core::DomainError;
using uno::core::DomainEvent;
using uno::core::DrawCard;
using uno::core::kHandSize;
using uno::core::Pass;
using uno::core::PenaltyCardsDrawn;
using uno::core::PenaltyResponse;
using uno::core::PlayCard;
using uno::core::Rank;
using uno::core::RespondPenalty;
using uno::core::Round;
using uno::core::RoundEnded;
using uno::core::RoundOver;
using uno::testing::coloredCard;
using uno::testing::deckGivingFirstHand;
using uno::testing::handOf;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

namespace {

constexpr std::uint64_t kSeed = 42;
constexpr std::uint32_t kLastCardId = 6;
constexpr std::uint32_t kOpponentHandPoints = 7 * 5; // seven red Fives

PlayCard playLastCard(std::optional<Color> color = std::nullopt)
{
    return PlayCard{.cardId = CardId{kLastCardId}, .chosenColor = color};
}

// Two players; red Two is flipped and player-0 starts with six red Skips (ids 0-5, each hands the
// turn straight back with two players) and `lastCard` (id 6). Returns the round with player-0 down
// to that last card. player-1 holds seven red Fives, worth 35 points; `afterTop` follows in the
// draw pile.
[[nodiscard]] Round roundWhereFirstPlayerHoldsOnly(const Card& lastCard, const std::vector<Card>& afterTop,
                                                   SeededRandomSource& random)
{
    std::vector<Card> hand;
    hand.reserve(kHandSize);
    for (std::uint32_t id = 0; id < kLastCardId; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Skip));
    }
    hand.push_back(lastCard);
    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, hand, coloredCard(40, Color::Red, Rank::Two), afterTop),
        },
        random);
    for (std::uint32_t id = 0; id < kLastCardId; ++id) {
        REQUIRE(
            round.apply(player(0), PlayCard{.cardId = CardId{id}, .chosenColor = std::nullopt}, random).has_value());
    }
    REQUIRE(handOf(round, player(0)).size() == 1);
    return round;
}

void requireTurnActionsRejected(Round& round, const uno::core::PlayerId& actor, SeededRandomSource& random)
{
    REQUIRE(round.apply(actor, DrawCard{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(round.apply(actor, Pass{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(round.apply(actor, PlayCard{.cardId = CardId{100}, .chosenColor = std::nullopt}, random).error() ==
            DomainError::InvalidPhase);
    REQUIRE(round.apply(actor, ChooseColor{.color = Color::Red}, random).error() == DomainError::InvalidPhase);
}

void requireSideActionsRejected(Round& round, const uno::core::PlayerId& actor, SeededRandomSource& random)
{
    REQUIRE(round.apply(actor, RespondPenalty{.response = PenaltyResponse::Accept}, random).error() ==
            DomainError::InvalidPhase);
    REQUIRE(round.apply(actor, CallUno{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(round.apply(actor, CatchUno{.target = player(0)}, random).error() == DomainError::InvalidPhase);
}

void requireEveryActionRejected(Round& round, const uno::core::PlayerId& actor, SeededRandomSource& random)
{
    requireTurnActionsRejected(round, actor, random);
    requireSideActionsRejected(round, actor, random);
}

} // namespace

TEST_CASE("Playing the last card ends the round and the winner scores the other hands", "[core][round][roundEnd]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWhereFirstPlayerHoldsOnly(coloredCard(kLastCardId, Color::Red, Rank::Skip), {}, random);

    const auto events = round.apply(player(0), playLastCard(), random);

    REQUIRE(events.has_value());
    // No PlayerSkipped, no TurnChanged: the round is over, nobody has a next turn.
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{kLastCardId}},
                           RoundEnded{.winner = player(0), .points = kOpponentHandPoints},
                       });
    REQUIRE(round.phase() == uno::core::TurnPhase{RoundOver{.winner = player(0), .points = kOpponentHandPoints}});
    REQUIRE(handOf(round, player(0)).empty());
    requireRoundInvariants(round);
}

TEST_CASE("A last Draw Two still makes the next player draw, and those cards count", "[core][round][roundEnd]")
{
    SeededRandomSource random{kSeed};
    // The drawn cards are worth 9 and 50.
    auto round =
        roundWhereFirstPlayerHoldsOnly(coloredCard(kLastCardId, Color::Red, Rank::DrawTwo),
                                       {coloredCard(41, Color::Green, Rank::Nine), wildCard(42, Rank::Wild)}, random);

    const auto events = round.apply(player(0), playLastCard(), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{kLastCardId}},
                           PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{42}}},
                           RoundEnded{.winner = player(0), .points = kOpponentHandPoints + 9 + 50},
                       });
    requireRoundInvariants(round);
}

TEST_CASE("A last Wild Draw Four makes the next player draw four without any challenge", "[core][round][roundEnd]")
{
    SeededRandomSource random{kSeed};
    // The drawn cards are worth 20, 20, 0 and 1.
    auto round = roundWhereFirstPlayerHoldsOnly(wildCard(kLastCardId, Rank::WildDrawFour),
                                                {
                                                    coloredCard(41, Color::Green, Rank::Skip),
                                                    coloredCard(42, Color::Green, Rank::Reverse),
                                                    coloredCard(43, Color::Green, Rank::Zero),
                                                    coloredCard(44, Color::Green, Rank::One),
                                                },
                                                random);

    const auto events = round.apply(player(0), playLastCard(Color::Blue), random);

    REQUIRE(events.has_value());
    REQUIRE(*events ==
            std::vector<DomainEvent>{
                CardPlayed{.player = player(0), .cardId = CardId{kLastCardId}},
                ColorChosen{.player = player(0), .color = Color::Blue},
                PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}}},
                RoundEnded{.winner = player(0), .points = kOpponentHandPoints + 20 + 20 + 0 + 1},
            });
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("A last Wild ends the round after choosing its color", "[core][round][roundEnd]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWhereFirstPlayerHoldsOnly(wildCard(kLastCardId, Rank::Wild), {}, random);

    const auto events = round.apply(player(0), playLastCard(Color::Green), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{kLastCardId}},
                           ColorChosen{.player = player(0), .color = Color::Green},
                           RoundEnded{.winner = player(0), .points = kOpponentHandPoints},
                       });
}

TEST_CASE("Winning leaves no UNO window behind", "[core][round][roundEnd]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWhereFirstPlayerHoldsOnly(coloredCard(kLastCardId, Color::Red, Rank::Skip), {}, random);
    // After the sixth Skip, player-0 left themselves with one card without announcing: window open.
    REQUIRE(round.hasUnoWindowOn(player(0)));

    REQUIRE(round.apply(player(0), playLastCard(), random).has_value());

    REQUIRE(round.unoWindows().empty());
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::InvalidPhase);
}

TEST_CASE("A finished round rejects every action", "[core][round][roundEnd]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWhereFirstPlayerHoldsOnly(coloredCard(kLastCardId, Color::Red, Rank::Skip), {}, random);
    REQUIRE(round.apply(player(0), playLastCard(), random).has_value());
    const auto before = round;

    requireEveryActionRejected(round, player(0), random);
    requireEveryActionRejected(round, player(1), random);
    REQUIRE(round == before);
}
