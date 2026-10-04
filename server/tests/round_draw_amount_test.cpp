#include "uno/core/card.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_amount.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// The draw amount (SPEC §4, ADR 0024): a player with nothing to play draws, in one action, until a card fits.

namespace {

using uno::core::AwaitingDrawnCardDecision;
using uno::core::Card;
using uno::core::CardId;
using uno::core::CardsDrawn;
using uno::core::Color;
using uno::core::DeckReshuffled;
using uno::core::DomainEvent;
using uno::core::DrawAmount;
using uno::core::DrawCard;
using uno::core::DrawRule;
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

constexpr std::uint64_t kSeed = 7;

// Two players; the flipped card is a red Two (current color red) and player-0 plays first, holding `firstCard` and six
// blue Fives. `drawPile` is the draw pile, in draw order.
[[nodiscard]] Round roundWith(DrawRule rule, DrawAmount amount, const Card& firstCard,
                              const std::vector<Card>& drawPile, SeededRandomSource& random)
{
    std::vector<Card> hand{firstCard};
    for (std::uint32_t id = 10; id < 16; ++id) {
        hand.push_back(coloredCard(id, Color::Blue, Rank::Five));
    }
    return startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, hand, coloredCard(40, Color::Red, Rank::Two), drawPile),
            .drawRule = rule,
            .drawAmount = amount,
        },
        random);
}

[[nodiscard]] Card blueCard(std::uint32_t id, Rank rank)
{
    return coloredCard(id, Color::Blue, rank);
}

[[nodiscard]] Card redCard(std::uint32_t id, Rank rank)
{
    return coloredCard(id, Color::Red, rank);
}

// A card that cannot be played on the red Two.
[[nodiscard]] Card unplayable()
{
    return blueCard(1, Rank::Six);
}

[[nodiscard]] std::vector<std::size_t> drawnPerEvent(const std::vector<DomainEvent>& events)
{
    std::vector<std::size_t> counts;
    for (const auto& event : events) {
        if (const auto* drawn = std::get_if<CardsDrawn>(&event)) {
            counts.push_back(drawn->cards.size());
        }
    }
    return counts;
}

[[nodiscard]] bool hasReshuffle(const std::vector<DomainEvent>& events)
{
    return std::ranges::any_of(events,
                               [](const DomainEvent& event) { return std::holds_alternative<DeckReshuffled>(event); });
}

} // namespace

TEST_CASE("UntilPlayable: draws until a card fits, one event per card", "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(
        DrawRule::Guided, DrawAmount::UntilPlayable, unplayable(),
        {blueCard(41, Rank::Seven), blueCard(42, Rank::Eight), redCard(43, Rank::Seven), redCard(44, Rank::Nine)},
        random);

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(drawnPerEvent(*events) == std::vector<std::size_t>{1, 1, 1});
    REQUIRE(handOf(round, player(0)).size() == 10);
    REQUIRE(std::holds_alternative<AwaitingDrawnCardDecision>(round.phase()));
    REQUIRE(round.forcedAction() ==
            std::optional<PlayerAction>{PlayCard{.cardId = CardId{43}, .chosenColor = std::nullopt}});
    requireRoundInvariants(round);
}

TEST_CASE("UntilPlayable: a voluntary draw stays a single card", "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Guided, DrawAmount::UntilPlayable, redCard(1, Rank::DrawTwo),
                           {blueCard(41, Rank::Seven), redCard(42, Rank::Seven)}, random);

    REQUIRE(round.canDraw(player(0)));
    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(drawnPerEvent(*events) == std::vector<std::size_t>{1});
    REQUIRE(handOf(round, player(0)).size() == 8);
    requireRoundInvariants(round);
}

TEST_CASE("One: a player with nothing to play draws a single card", "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Guided, DrawAmount::One, unplayable(),
                           {blueCard(41, Rank::Seven), redCard(42, Rank::Seven)}, random);

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(drawnPerEvent(*events) == std::vector<std::size_t>{1});
    REQUIRE(round.currentPlayer() == player(1));
}

TEST_CASE("UntilPlayable works with the official draw rule, and the player may keep the card",
          "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Official, DrawAmount::UntilPlayable, unplayable(),
                           {blueCard(41, Rank::Seven), redCard(42, Rank::Seven)}, random);

    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    REQUIRE(std::holds_alternative<AwaitingDrawnCardDecision>(round.phase()));
    REQUIRE(round.canKeepDrawnCard(player(0)));
    REQUIRE(round.forcedAction() == std::nullopt);
}

TEST_CASE("UntilPlayable: when both piles run out, the loop ends and the turn passes", "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Guided, DrawAmount::UntilPlayable, unplayable(),
                           {blueCard(41, Rank::Seven), blueCard(42, Rank::Eight)}, random);

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(drawnPerEvent(*events) == std::vector<std::size_t>{1, 1});
    REQUIRE(handOf(round, player(0)).size() == 9);
    REQUIRE(round.drawPile().empty());
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("UntilPlayable: with nothing left to draw at all, the turn passes", "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWith(DrawRule::Guided, DrawAmount::UntilPlayable, unplayable(), {}, random);

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(drawnPerEvent(*events) == std::vector<std::size_t>{0});
    REQUIRE(round.currentPlayer() == player(1));
}

TEST_CASE("UntilPlayable: an empty draw pile is refilled from the discard pile, keeping the top card",
          "[core][round][drawAmount]")
{
    SeededRandomSource random{kSeed};
    // Player-0 plays a red Skip (two players: they play again) and has nothing red left; the draw pile holds one blue
    // card and the discard pile the flipped red Two under the Skip.
    auto round = roundWith(DrawRule::Guided, DrawAmount::UntilPlayable, redCard(1, Rank::Skip),
                           {blueCard(41, Rank::Seven)}, random);
    REQUIRE(round.apply(player(0), PlayCard{.cardId = CardId{1}, .chosenColor = std::nullopt}, random).has_value());
    REQUIRE(round.currentPlayer() == player(0));

    const auto events = round.apply(player(0), DrawCard{}, random);

    REQUIRE(events.has_value());
    REQUIRE(drawnPerEvent(*events) == std::vector<std::size_t>{1, 1});
    REQUIRE(hasReshuffle(*events));
    REQUIRE(round.discardPile().top().id == CardId{1});
    REQUIRE(std::holds_alternative<AwaitingDrawnCardDecision>(round.phase()));
    requireRoundInvariants(round);
}
