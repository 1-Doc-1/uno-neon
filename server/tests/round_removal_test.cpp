#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// Taking a player out of a round or a match for good (SPEC §5, step 2.5).

namespace {

using namespace uno::core;
using uno::testing::coloredCard;
using uno::testing::deckGivingFirstHand;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

constexpr std::uint64_t kSeed = 21;

std::vector<Card> redFives()
{
    std::vector<Card> hand;
    for (std::uint32_t id = 0; hand.size() < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Five));
    }
    return hand;
}

// `count` players; the first (who plays first) holds `firstHand`; the draw pile holds 10 yellow Threes.
Round roundOf(std::size_t count, const std::vector<Card>& firstHand, const Card& top, SeededRandomSource& random)
{
    std::vector<Card> drawPile;
    for (std::uint32_t id = 300; drawPile.size() < 10; ++id) {
        drawPile.push_back(coloredCard(id, Color::Yellow, Rank::Three));
    }
    return startedRound(
        {
            .seats = players(count),
            .dealer = player(count - 1),
            .deck = deckGivingFirstHand(count, firstHand, top, drawPile),
        },
        random);
}

Card blueTwo()
{
    return coloredCard(40, Color::Blue, Rank::Two);
}

} // namespace

TEST_CASE("A removed player leaves with their cards, which go under the draw pile", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(4, redFives(), blueTwo(), random);
    const auto pileBefore = round.drawPile().size();

    const auto events = round.removePlayer(player(2), random);

    REQUIRE(events.has_value());
    REQUIRE(round.seats().size() == 3);
    REQUIRE(round.drawPile().size() == pileBefore + kHandSize);
    REQUIRE(round.hand(player(2)).error() == DomainError::UnknownPlayer);
    // They sit at the bottom: the next draw is still the card that was on top.
    REQUIRE(round.drawPile().cards().back().id == CardId{300});
    REQUIRE(round.drawPile().cards().front().color == Color::Red);
    requireRoundInvariants(round);
}

TEST_CASE("When the current player is removed the turn passes to the next one, in the direction of play",
          "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(4, redFives(), blueTwo(), random);
    REQUIRE(round.currentPlayer() == player(0));

    const auto events = round.removePlayer(player(0), random);

    REQUIRE(events.has_value());
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(*events == std::vector<DomainEvent>{TurnChanged{.player = player(1)}});
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("Counter-clockwise, the turn passes to the previous seat", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto hand = redFives();
    hand.front() = coloredCard(0, Color::Blue, Rank::Reverse);
    auto round = roundOf(4, hand, blueTwo(), random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);
    REQUIRE(played.has_value());
    REQUIRE(round.currentPlayer() == player(3));
    REQUIRE(round.direction() == Direction::CounterClockwise);

    const auto events = round.removePlayer(player(3), random);

    REQUIRE(events.has_value());
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("Removing someone else keeps the turn where it is", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(4, redFives(), blueTwo(), random);

    const auto events = round.removePlayer(player(3), random);

    REQUIRE(events.has_value());
    REQUIRE(events->empty());
    REQUIRE(round.currentPlayer() == player(0));
    requireRoundInvariants(round);
}

TEST_CASE("Removing a player before the current seat keeps the same current player", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto hand = redFives();
    hand.front() = coloredCard(0, Color::Blue, Rank::Three);
    auto round = roundOf(4, hand, blueTwo(), random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);
    REQUIRE(played.has_value());
    REQUIRE(round.currentPlayer() == player(1));

    const auto events = round.removePlayer(player(0), random);

    REQUIRE(events.has_value());
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("Two players cannot be reduced to one by a round", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(2, redFives(), blueTwo(), random);

    REQUIRE(round.removePlayer(player(1), random).error() == DomainError::NotEnoughPlayers);
    REQUIRE(round.seats().size() == 2);
}

TEST_CASE("An unknown player cannot be removed", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(3, redFives(), blueTwo(), random);

    REQUIRE(round.removePlayer(player(9), random).error() == DomainError::UnknownPlayer);
}

TEST_CASE("A Wild Draw Four whose target leaves is void", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto hand = redFives();
    hand.front() = wildCard(0, Rank::WildDrawFour);
    auto round = roundOf(3, hand, blueTwo(), random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());
    REQUIRE(std::holds_alternative<AwaitingPenaltyResponse>(round.phase()));

    const auto events = round.removePlayer(player(1), random);

    REQUIRE(events.has_value());
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("A Wild Draw Four whose poser leaves is void too, and the target plays on", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto hand = redFives();
    hand.front() = wildCard(0, Rank::WildDrawFour);
    auto round = roundOf(3, hand, blueTwo(), random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());

    const auto events = round.removePlayer(player(0), random);

    REQUIRE(events.has_value());
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("A player who drew a playable card and leaves hands the turn over", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(3, redFives(), blueTwo(), random);
    // The next card of the draw pile (yellow Three) is not playable on a blue Two: draw one that is.
    const auto drawn = round.apply(player(0), DrawCard{}, random);
    REQUIRE(drawn.has_value());
    REQUIRE(round.currentPlayer() == player(1)); // not playable: the turn already moved on

    const auto events = round.removePlayer(player(1), random);

    REQUIRE(events.has_value());
    REQUIRE(round.currentPlayer() == player(2));
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("A first Wild without a color gets a random one when its chooser leaves", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(3, redFives(), wildCard(40, Rank::Wild), random);
    REQUIRE(std::holds_alternative<AwaitingColorChoice>(round.phase()));
    REQUIRE_FALSE(round.currentColor().has_value());

    const auto events = round.removePlayer(player(0), random);

    REQUIRE(events.has_value());
    REQUIRE(round.currentColor().has_value());
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(std::ranges::any_of(*events,
                                [](const DomainEvent& event) { return std::holds_alternative<ColorChosen>(event); }));
    requireRoundInvariants(round);
}

TEST_CASE("The dealer who leaves is replaced by the previous seat", "[core][removal]")
{
    SeededRandomSource random{kSeed};
    auto round = roundOf(4, redFives(), blueTwo(), random);
    REQUIRE(round.dealer() == player(3));

    const auto events = round.removePlayer(player(3), random);

    REQUIRE(events.has_value());
    REQUIRE(round.dealer() == player(2));
}

TEST_CASE("A match of two ends by forfeit when one player leaves", "[core][removal][match]")
{
    SeededRandomSource random{kSeed};
    auto started = Match::start(players(2), MatchSettings(), random);
    REQUIRE(started.has_value());
    auto match = std::move(started->match);

    const auto events = match.removePlayer(player(0), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{MatchEnded{.winner = player(1)}});
    REQUIRE(match.winner() == player(1));
    REQUIRE(match.apply(player(1), DrawCard{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(match.removePlayer(player(1), random).error() == DomainError::InvalidPhase);
}

TEST_CASE("A match of three goes on with two, scores kept in step with the seats", "[core][removal][match]")
{
    SeededRandomSource random{kSeed};
    auto started = Match::start(players(3), MatchSettings(), random);
    REQUIRE(started.has_value());
    auto match = std::move(started->match);

    const auto events = match.removePlayer(player(1), random);

    REQUIRE(events.has_value());
    REQUIRE(match.round().seats().size() == 2);
    REQUIRE(match.progress().scores.size() == 2);
    REQUIRE(match.score(player(1)).error() == DomainError::UnknownPlayer);
    REQUIRE(match.score(player(2)).has_value());
    requireRoundInvariants(match.round());
}

TEST_CASE("Nobody unknown leaves a match", "[core][removal][match]")
{
    SeededRandomSource random{kSeed};
    auto started = Match::start(players(3), MatchSettings(), random);
    REQUIRE(started.has_value());
    auto match = std::move(started->match);

    REQUIRE(match.removePlayer(player(7), random).error() == DomainError::UnknownPlayer);
}
