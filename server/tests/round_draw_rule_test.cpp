#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
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

// The guided draw (SPEC §4, ADR 0017): what the engine allows, and the one move it reports as forced.

namespace {

using uno::core::AwaitingDrawnCardDecision;
using uno::core::AwaitingPlay;
using uno::core::Card;
using uno::core::Color;
using uno::core::DomainError;
using uno::core::DrawCard;
using uno::core::DrawRule;
using uno::core::Pass;
using uno::core::PlayCard;
using uno::core::PlayerAction;
using uno::core::Rank;
using uno::core::Round;
using uno::testing::coloredCard;
using uno::testing::deckGivingFirstHand;
using uno::testing::handOf;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

constexpr std::uint64_t kSeed = 7;

// Two players; the flipped card is a red Two, so the current color is red and player-0 plays first. Player-0 holds
// `firstCard` and six blue Fives (never playable on a red Two); `drawn` is the next card of the draw pile.
[[nodiscard]] Round roundWith(DrawRule rule, const Card& firstCard, const Card& drawn, SeededRandomSource& random)
{
    std::vector<Card> hand{firstCard};
    for (std::uint32_t id = 10; id < 16; ++id) {
        hand.push_back(coloredCard(id, Color::Blue, Rank::Five));
    }
    return startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, hand, coloredCard(40, Color::Red, Rank::Two),
                                        {drawn, coloredCard(41, Color::Green, Rank::Nine)}),
            .drawRule = rule,
        },
        random);
}

[[nodiscard]] Round roundWithNothingPlayable(DrawRule rule, const Card& drawn, SeededRandomSource& random)
{
    return roundWith(rule, coloredCard(1, Color::Blue, Rank::Six), drawn, random);
}

[[nodiscard]] Card redSeven()
{
    return coloredCard(50, Color::Red, Rank::Seven);
}

[[nodiscard]] Card blueSeven()
{
    return coloredCard(51, Color::Blue, Rank::Seven);
}

[[nodiscard]] Card redDrawTwo()
{
    return coloredCard(52, Color::Red, Rank::DrawTwo);
}

} // namespace

TEST_CASE("Guided: with nothing playable, drawing is allowed and is the forced move", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithNothingPlayable(DrawRule::Guided, blueSeven(), random);

    REQUIRE(round.canDraw(player(0)));
    REQUIRE(round.forcedAction() == std::optional<PlayerAction>{DrawCard{}});
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());
    requireRoundInvariants(round);
}

TEST_CASE("Guided: a player who can play a plain card has to, and cannot draw", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Guided, coloredCard(1, Color::Red, Rank::Five), blueSeven(), random);

    REQUIRE_FALSE(round.canDraw(player(0)));
    REQUIRE(round.forcedAction() == std::nullopt);
    REQUIRE(round.apply(player(0), DrawCard{}, random).error() == DomainError::MustPlay);
    REQUIRE(handOf(round, player(0)).size() == 7);
    requireRoundInvariants(round);
}

TEST_CASE("Guided: Skip and Reverse count as plain cards", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    for (const auto rank : {Rank::Skip, Rank::Reverse}) {
        auto round = roundWith(DrawRule::Guided, coloredCard(1, Color::Red, rank), blueSeven(), random);

        REQUIRE_FALSE(round.canDraw(player(0)));
    }
}

TEST_CASE("Guided: a playable special card leaves the choice between playing and drawing", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    for (const auto& special : {redDrawTwo(), wildCard(53, Rank::Wild), wildCard(54, Rank::WildDrawFour)}) {
        auto round = roundWith(DrawRule::Guided, special, blueSeven(), random);

        REQUIRE(round.canDraw(player(0)));
        REQUIRE(round.forcedAction() == std::nullopt);
        REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());
    }
}

TEST_CASE("Guided: a drawn card that does not fit ends the turn", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithNothingPlayable(DrawRule::Guided, blueSeven(), random);

    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("Guided: a drawn plain card that fits has to be played, and the engine says so", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithNothingPlayable(DrawRule::Guided, redSeven(), random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    REQUIRE(std::holds_alternative<AwaitingDrawnCardDecision>(round.phase()));
    REQUIRE_FALSE(round.canKeepDrawnCard(player(0)));
    REQUIRE(round.forcedAction() ==
            std::optional<PlayerAction>{PlayCard{.cardId = redSeven().id, .chosenColor = std::nullopt}});
    REQUIRE(round.apply(player(0), Pass{}, random).error() == DomainError::MustPlay);
    REQUIRE(round.apply(player(0), *round.forcedAction(), random).has_value());
    REQUIRE(round.discardPile().top().id == redSeven().id);
    requireRoundInvariants(round);
}

TEST_CASE("Guided: a drawn special card that fits may be played or kept", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithNothingPlayable(DrawRule::Guided, redDrawTwo(), random);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    REQUIRE(round.canKeepDrawnCard(player(0)));
    REQUIRE(round.forcedAction() == std::nullopt);
    REQUIRE(round.apply(player(0), Pass{}, random).has_value());
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(handOf(round, player(0)).size() == 8);
}

TEST_CASE("Official: drawing and keeping are always allowed, and nothing is forced", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Official, coloredCard(1, Color::Red, Rank::Five), redSeven(), random);

    REQUIRE(round.canDraw(player(0)));
    REQUIRE(round.forcedAction() == std::nullopt);
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());
    REQUIRE(round.canKeepDrawnCard(player(0)));
    REQUIRE(round.forcedAction() == std::nullopt);
    REQUIRE(round.apply(player(0), Pass{}, random).has_value());
}

TEST_CASE("Only the current player can draw or keep, whatever the rule", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithNothingPlayable(DrawRule::Guided, redDrawTwo(), random);

    REQUIRE_FALSE(round.canDraw(player(1)));
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());
    REQUIRE_FALSE(round.canKeepDrawnCard(player(1)));
}

TEST_CASE("The legal actions of the testing helpers follow the draw rule", "[core][round][drawRule]")
{
    SeededRandomSource random{kSeed};
    const auto hasDraw = [](const std::vector<PlayerAction>& actions) {
        return std::ranges::any_of(actions,
                                   [](const PlayerAction& action) { return std::holds_alternative<DrawCard>(action); });
    };
    auto mustPlay = roundWith(DrawRule::Guided, coloredCard(1, Color::Red, Rank::Five), blueSeven(), random);
    auto mayDraw = roundWithNothingPlayable(DrawRule::Guided, blueSeven(), random);

    REQUIRE_FALSE(hasDraw(uno::testing::legalActionsOfCurrentPlayer(mustPlay)));
    REQUIRE(hasDraw(uno::testing::legalActionsOfCurrentPlayer(mayDraw)));
}
