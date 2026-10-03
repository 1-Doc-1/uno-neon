#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/legal_actions.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// The house rule "declare UNO to win" (SPEC §4, ADR 0019): the last card cannot be played without having announced UNO.

namespace {

using uno::core::AwaitingPlay;
using uno::core::CallUno;
using uno::core::Card;
using uno::core::CardId;
using uno::core::Color;
using uno::core::DomainError;
using uno::core::DrawCard;
using uno::core::DrawRule;
using uno::core::kHandSize;
using uno::core::PlayCard;
using uno::core::Rank;
using uno::core::Round;
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

constexpr std::uint64_t kSeed = 42;
constexpr std::uint32_t kSkipCount = 6;
constexpr std::uint32_t kLastCardId = 6;

// Two players; red Two is flipped. Player-0 holds six red Skips (each hands the turn straight back with two players)
// and `lastCard` (id 6), so that after the six Skips they are down to that one card.
[[nodiscard]] Round roundBeforeLastCard(const Card& lastCard, bool rule, DrawRule drawRule, SeededRandomSource& random)
{
    std::vector<Card> hand;
    hand.reserve(kHandSize);
    for (std::uint32_t id = 0; id < kSkipCount; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Skip));
    }
    hand.push_back(lastCard);
    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, hand, coloredCard(40, Color::Red, Rank::Two),
                                        {coloredCard(41, Color::Green, Rank::Seven)}),
            .drawRule = drawRule,
            .declareUnoToWin = rule,
        },
        random);
    for (std::uint32_t id = 0; id < kSkipCount; ++id) {
        REQUIRE(
            round.apply(player(0), PlayCard{.cardId = CardId{id}, .chosenColor = std::nullopt}, random).has_value());
    }
    REQUIRE(handOf(round, player(0)).size() == 1);
    return round;
}

[[nodiscard]] Card redFive()
{
    return coloredCard(kLastCardId, Color::Red, Rank::Five);
}

[[nodiscard]] PlayCard playLast(std::optional<Color> color = std::nullopt)
{
    return PlayCard{.cardId = CardId{kLastCardId}, .chosenColor = color};
}

} // namespace

TEST_CASE("With the rule, the last card is refused until UNO has been announced", "[core][round][declareUno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(redFive(), true, DrawRule::Official, random);

    REQUIRE(round.mustDeclareUno(player(0)));
    REQUIRE(round.apply(player(0), playLast(), random).error() == DomainError::MustDeclareUno);
    REQUIRE(handOf(round, player(0)).size() == 1);
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));

    REQUIRE(round.apply(player(0), CallUno{}, random).has_value());
    REQUIRE_FALSE(round.mustDeclareUno(player(0)));
    REQUIRE(round.apply(player(0), playLast(), random).has_value());
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("An announcement made with two cards still counts for the last one", "[core][round][declareUno]")
{
    SeededRandomSource again{kSeed};
    std::vector<Card> hand;
    hand.reserve(kHandSize);
    for (std::uint32_t id = 0; id < kSkipCount; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Skip));
    }
    hand.push_back(redFive());
    auto early = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, hand, coloredCard(40, Color::Red, Rank::Two), {}),
            .drawRule = DrawRule::Official,
            .declareUnoToWin = true,
        },
        again);
    for (std::uint32_t id = 0; id + 2 < kSkipCount; ++id) {
        REQUIRE(early.apply(player(0), PlayCard{.cardId = CardId{id}, .chosenColor = std::nullopt}, again).has_value());
    }
    REQUIRE(handOf(early, player(0)).size() == 3);
    REQUIRE(early.apply(player(0), PlayCard{.cardId = CardId{4}, .chosenColor = std::nullopt}, again).has_value());
    REQUIRE(handOf(early, player(0)).size() == 2);
    REQUIRE(early.apply(player(0), CallUno{}, again).has_value());
    REQUIRE(early.apply(player(0), PlayCard{.cardId = CardId{5}, .chosenColor = std::nullopt}, again).has_value());
    REQUIRE(handOf(early, player(0)).size() == 1);

    REQUIRE_FALSE(early.mustDeclareUno(player(0)));
    REQUIRE(early.apply(player(0), playLast(), again).has_value());
    REQUIRE(std::holds_alternative<RoundOver>(early.phase()));
}

TEST_CASE("Without the rule the last card is played freely", "[core][round][declareUno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(redFive(), false, DrawRule::Official, random);

    REQUIRE_FALSE(round.mustDeclareUno(player(0)));
    REQUIRE(round.apply(player(0), playLast(), random).has_value());
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
}

TEST_CASE("A last card that cannot be played reports its own error, and nothing is blocked",
          "[core][round][declareUno]")
{
    SeededRandomSource random{kSeed};
    auto round =
        roundBeforeLastCard(coloredCard(kLastCardId, Color::Blue, Rank::Five), true, DrawRule::Official, random);

    REQUIRE_FALSE(round.mustDeclareUno(player(0)));
    REQUIRE(round.apply(player(0), playLast(), random).error() == DomainError::ColorMismatch);
}

TEST_CASE("A Wild as the last card is refused too", "[core][round][declareUno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(wildCard(kLastCardId, Rank::Wild), true, DrawRule::Official, random);

    REQUIRE(round.mustDeclareUno(player(0)));
    REQUIRE(round.apply(player(0), playLast(Color::Blue), random).error() == DomainError::MustDeclareUno);
}

TEST_CASE("Guided draw: a playable last card blocked by the missing UNO never triggers the automatic draw",
          "[core][round][declareUno][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(redFive(), true, DrawRule::Guided, random);

    REQUIRE(round.mustDeclareUno(player(0)));
    REQUIRE(round.forcedAction() == std::nullopt);
    REQUIRE_FALSE(round.canDraw(player(0)));
    REQUIRE(round.apply(player(0), DrawCard{}, random).error() == DomainError::MustPlay);
}

TEST_CASE("Guided draw: a last card that does not fit is still drawn for", "[core][round][declareUno][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(coloredCard(kLastCardId, Color::Blue, Rank::Five), true, DrawRule::Guided, random);

    REQUIRE_FALSE(round.mustDeclareUno(player(0)));
    REQUIRE(round.forcedAction().has_value());
}

TEST_CASE("The legal actions offered while blocked are the announcement, not the refused card",
          "[core][round][declareUno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(redFive(), true, DrawRule::Guided, random);

    const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);

    REQUIRE(actions == std::vector<uno::core::PlayerAction>{CallUno{}});
}

TEST_CASE("Only the player on turn is told they must declare UNO", "[core][round][declareUno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundBeforeLastCard(redFive(), true, DrawRule::Official, random);

    REQUIRE_FALSE(round.mustDeclareUno(player(1)));
    REQUIRE_FALSE(round.mustDeclareUno(player(7)));
}
