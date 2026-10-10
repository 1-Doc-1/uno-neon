#include "uno/core/card.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/seeded_random_source.hpp"
#include "uno/testing/view_leak_check.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// The projection of events for one viewer (step 2.4, ADR 0015) and its anti-leak guarantees.

namespace {

using namespace uno::core;
using uno::testing::coloredCard;
using uno::testing::deckGivingFirstHand;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireEventsLeakNothing;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::startRound;
using uno::testing::wildCard;

constexpr std::uint64_t kSeed = 11;
constexpr std::uint32_t kRoundNumber = 3;

template <typename Event>
std::vector<Event> eventsOfKind(const std::vector<ClientEvent>& events)
{
    std::vector<Event> found;
    for (const auto& event : events) {
        if (const auto* typed = std::get_if<Event>(&event)) {
            found.push_back(*typed);
        }
    }
    return found;
}

// Player 0 holds a Wild Draw Four and a blue card while blue is current: playing the +4 is a bluff.
Round bluffingRound(SeededRandomSource& random)
{
    std::vector<Card> hand{wildCard(0, Rank::WildDrawFour), coloredCard(1, Color::Blue, Rank::Seven)};
    for (std::uint32_t id = 2; hand.size() < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Five));
    }
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    std::vector<Card> drawPile; // enough to pay the penalty without reshuffling
    for (std::uint32_t id = 200; drawPile.size() < 10; ++id) {
        drawPile.push_back(coloredCard(id, Color::Yellow, Rank::Three));
    }
    return startedRound({.seats = players(3), .dealer = player(2), .deck = deckGivingFirstHand(3, hand, top, drawPile)},
                        random);
}

std::vector<DomainEvent> playBluffAndChallenge(Round& round, SeededRandomSource& random)
{
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());
    auto events = *played;
    const auto challenged = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random);
    REQUIRE(challenged.has_value());
    events.insert(events.end(), challenged->begin(), challenged->end());
    return events;
}

} // namespace

TEST_CASE("Only the challenger sees the hand a challenge reveals", "[core][clientEvent][leak]")
{
    SeededRandomSource random{kSeed};
    auto round = bluffingRound(random);
    const auto events = playBluffAndChallenge(round, random);

    for (std::size_t seat = 0; seat < 3; ++seat) {
        const auto viewer = player(seat);
        const auto projected = project(events, viewer, round, kRoundNumber);

        const auto challenges = eventsOfKind<ChallengeResolvedEvent>(projected);
        REQUIRE(challenges.size() == 1);
        REQUIRE(challenges.front().wasBluff);
        REQUIRE(challenges.front().revealedHand.has_value() == (seat == 1));
        requireEventsLeakNothing(projected, round, viewer);
    }
}

TEST_CASE("The revealed hand is the one the poser held when playing", "[core][clientEvent]")
{
    SeededRandomSource random{kSeed};
    auto round = bluffingRound(random);
    const auto events = playBluffAndChallenge(round, random);

    const auto projected = project(events, player(1), round, kRoundNumber);

    const auto revealed =
        eventsOfKind<ChallengeResolvedEvent>(projected).front().revealedHand.value_or(std::vector<Card>{});
    REQUIRE(revealed.size() == kHandSize - 1); // the Wild Draw Four itself is already on the pile
    REQUIRE(std::ranges::any_of(revealed, [](const Card& card) { return card.id == CardId{1}; }));
}

TEST_CASE("Cards drawn are shown to the player who drew them, the others only see a count", "[core][clientEvent][leak]")
{
    SeededRandomSource random{kSeed};
    auto round = bluffingRound(random);
    const auto events = playBluffAndChallenge(round, random);

    for (std::size_t seat = 0; seat < 3; ++seat) {
        const auto projected = project(events, player(seat), round, kRoundNumber);

        const auto draws = eventsOfKind<CardsDrawnEvent>(projected);
        REQUIRE(draws.size() == 1); // the poser draws 4 as a penalty
        REQUIRE(draws.front().playerId == player(0));
        REQUIRE(draws.front().count == 4);
        REQUIRE(draws.front().cards.has_value() == (seat == 0));
        requireEventsLeakNothing(projected, round, player(seat));
    }
}

TEST_CASE("A played Wild shows its card and chosen color to everyone", "[core][clientEvent]")
{
    SeededRandomSource random{kSeed};
    auto round = bluffingRound(random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());

    const auto projected = project(*played, player(2), round, kRoundNumber);

    const auto cards = eventsOfKind<CardPlayedEvent>(projected);
    REQUIRE(cards.size() == 1);
    REQUIRE(cards.front().card == wildCard(0, Rank::WildDrawFour));
    REQUIRE(cards.front().chosenColor == Color::Green);
    REQUIRE_FALSE(cards.front().isJumpIn);
    REQUIRE(eventsOfKind<ColorChosenEvent>(projected).size() == 1);
}

TEST_CASE("A plain card has no chosen color and a Reverse reports the new direction", "[core][clientEvent]")
{
    SeededRandomSource random{kSeed};
    std::vector<Card> hand{coloredCard(0, Color::Blue, Rank::Reverse)};
    for (std::uint32_t id = 1; hand.size() < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Five));
    }
    auto round = startedRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck = deckGivingFirstHand(3, hand, coloredCard(40, Color::Blue, Rank::Two)),
        },
        random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);
    REQUIRE(played.has_value());

    const auto projected = project(*played, player(1), round, kRoundNumber);

    REQUIRE(eventsOfKind<CardPlayedEvent>(projected).front().chosenColor == std::nullopt);
    const auto directions = eventsOfKind<DirectionChangedEvent>(projected);
    REQUIRE(directions.size() == 1);
    REQUIRE(directions.front().direction == Direction::CounterClockwise);
}

TEST_CASE("The start of a round is projected with the number of the round", "[core][clientEvent]")
{
    SeededRandomSource random{kSeed};
    std::vector<Card> hand;
    for (std::uint32_t id = 0; hand.size() < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Five));
    }
    const auto start = startRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck = deckGivingFirstHand(3, hand, coloredCard(40, Color::Blue, Rank::Two)),
        },
        random);

    const auto projected = project(start.events, player(0), start.round, kRoundNumber);

    const auto started = eventsOfKind<RoundStartedEvent>(projected);
    REQUIRE(started.size() == 1);
    REQUIRE(started.front().round == kRoundNumber);
    REQUIRE(started.front().dealerId == player(2));
}

TEST_CASE("A pass has no event of its own for the clients", "[core][clientEvent]")
{
    const std::vector<DomainEvent> events{TurnPassed{.player = player(0)}, TurnChanged{.player = player(1)}};
    SeededRandomSource random{kSeed};
    const auto round = bluffingRound(random);

    const auto projected = project(events, player(0), round, kRoundNumber);

    REQUIRE(projected.size() == 1);
    REQUIRE(std::holds_alternative<TurnChangedEvent>(projected.front()));
}

TEST_CASE("A Wild Draw Five names its target and the total to everybody, whoever they are", "[core][clientEvent][leak]")
{
    SeededRandomSource random{kSeed};
    std::vector<Card> hand{wildCard(0, Rank::WildDrawFive)};
    for (std::uint32_t id = 1; hand.size() < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Five));
    }
    auto round = startedRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck = deckGivingFirstHand(3, hand, coloredCard(40, Color::Blue, Rank::Two)),
        },
        random);
    const auto played =
        round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green, .target = player(2)}, random);
    REQUIRE(played.has_value());

    const PlusFiveTargetedEvent expected{.playerId = player(0), .targetId = player(2), .total = 5};
    for (const auto& viewer : players(3)) {
        const auto projected = project(*played, viewer, round, kRoundNumber);
        const auto targeted = eventsOfKind<PlusFiveTargetedEvent>(projected);
        REQUIRE(targeted == std::vector<PlusFiveTargetedEvent>{expected});
        requireEventsLeakNothing(projected, round, viewer);
    }
}
