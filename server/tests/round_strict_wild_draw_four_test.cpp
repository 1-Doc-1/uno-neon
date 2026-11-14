#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/penalty_stacking.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/core/wild_draw_four_mode.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// The strict Wild Draw Four (ADR 0041): a Wild Draw Four whose hand holds a card of the current color is refused, so a
// Wild Draw Four that is played is always legal and there is nothing to challenge.

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
constexpr std::uint32_t kWildDrawFour = 0;

[[nodiscard]] std::vector<Card> sevenCards(std::uint32_t firstId, const std::vector<Card>& given)
{
    std::vector<Card> hand = given;
    hand.reserve(kHandSize);
    for (auto slot = static_cast<std::uint32_t>(given.size()); slot < kHandSize; ++slot) {
        hand.push_back(coloredCard(firstId + slot, Color::Red, Rank::Five));
    }
    return hand;
}

// Three players, seat 0 to play on a blue Two. `given` are the first cards of each hand (the rest is red Fives).
[[nodiscard]] Round table(const std::vector<std::vector<Card>>& given, SeededRandomSource& random,
                          WildDrawFourMode mode, PenaltyStacking stacking = PenaltyStacking::Official,
                          const std::vector<Card>& afterTop = {})
{
    std::vector<std::vector<Card>> hands;
    hands.reserve(3);
    for (std::size_t seat = 0; seat < 3; ++seat) {
        hands.push_back(sevenCards(static_cast<std::uint32_t>(1000 * (seat + 1)),
                                   seat < given.size() ? given.at(seat) : std::vector<Card>{}));
    }
    return startedRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck = deckFromHands(hands, coloredCard(40, Color::Blue, Rank::Two), afterTop),
            .stacking = stacking,
            .wildDrawFourMode = mode,
        },
        random);
}

[[nodiscard]] PlayCard playWildDrawFour(std::uint32_t id = kWildDrawFour)
{
    return PlayCard{.cardId = CardId{id}, .chosenColor = Color::Green, .target = std::nullopt};
}

// A Wild Draw Four held next to a blue card, on a blue Two: illegal. Without the blue card: legal.
[[nodiscard]] std::vector<Card> illegalHand()
{
    return {wildCard(kWildDrawFour, Rank::WildDrawFour), coloredCard(1, Color::Blue, Rank::Nine)};
}

[[nodiscard]] std::vector<Card> legalHand()
{
    return {wildCard(kWildDrawFour, Rank::WildDrawFour)};
}

} // namespace

TEST_CASE("Strict: a Wild Draw Four is refused while the hand holds a card of the current color, and nothing changes",
          "[core][round][strict]")
{
    SeededRandomSource random{kSeed};
    auto round = table({illegalHand()}, random, WildDrawFourMode::Strict);
    const Round before = round;

    const auto result = round.apply(player(0), playWildDrawFour(), random);

    REQUIRE(result.error() == DomainError::WildDrawFourIllegal);
    REQUIRE(round == before);
}

TEST_CASE("Official: the same Wild Draw Four can be played, as a bluff to be challenged", "[core][round][strict]")
{
    SeededRandomSource random{kSeed};
    auto round = table({illegalHand()}, random, WildDrawFourMode::OfficialChallenge);

    const auto result = round.apply(player(0), playWildDrawFour(), random);

    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<AwaitingPenaltyResponse>(round.phase()));
}

TEST_CASE("Strict: a legal Wild Draw Four is drawn at once, with nothing to challenge", "[core][round][strict]")
{
    SeededRandomSource random{kSeed};
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),
        coloredCard(42, Color::Green, Rank::Two),
        coloredCard(43, Color::Green, Rank::Three),
        coloredCard(44, Color::Green, Rank::Four),
    };
    auto round = table({legalHand()}, random, WildDrawFourMode::Strict, PenaltyStacking::Official, afterTop);

    const auto events = round.apply(player(0), playWildDrawFour(), random);

    REQUIRE(events.has_value());
    REQUIRE(*events ==
            std::vector<DomainEvent>{
                CardPlayed{.player = player(0), .cardId = CardId{kWildDrawFour}},
                ColorChosen{.player = player(0), .color = Color::Green},
                PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}}},
                PlayerSkipped{.skippedPlayer = player(1)},
                TurnChanged{.player = player(2)},
            });
    REQUIRE(std::holds_alternative<AwaitingPlay>(round.phase()));
    REQUIRE(round.currentColor() == Color::Green);
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 4);
    requireRoundInvariants(round);
}

TEST_CASE("Strict: canPlayCard follows the hand, and only for a Wild Draw Four", "[core][round][strict]")
{
    SeededRandomSource random{kSeed};
    const auto wildDrawFour = wildCard(kWildDrawFour, Rank::WildDrawFour);
    const auto strict = table({illegalHand()}, random, WildDrawFourMode::Strict);
    const auto official = table({illegalHand()}, random, WildDrawFourMode::OfficialChallenge);
    const auto legal = table({legalHand()}, random, WildDrawFourMode::Strict);

    REQUIRE_FALSE(strict.canPlayCard(player(0), wildDrawFour));
    REQUIRE(strict.canPlayCard(player(0), coloredCard(1, Color::Blue, Rank::Nine)));
    REQUIRE(strict.canPlayCard(player(0), wildCard(2, Rank::Wild)));
    REQUIRE(official.canPlayCard(player(0), wildDrawFour));
    REQUIRE(legal.canPlayCard(player(0), wildDrawFour));
    REQUIRE_FALSE(legal.canPlayCard(player(9), wildDrawFour)); // not seated
}

TEST_CASE("Strict with the ladder: a Wild Draw Four on a pending penalty ignores the legality of the hand",
          "[core][round][strict]")
{
    SeededRandomSource random{kSeed};
    // Seat 0 plays a blue Draw Two; seat 1 holds a Wild Draw Four AND a blue card (illegal as a fresh play)
    const std::vector<Card> poserHand{coloredCard(0, Color::Blue, Rank::DrawTwo)};
    const std::vector<Card> targetHand{wildCard(2000, Rank::WildDrawFour), coloredCard(2001, Color::Blue, Rank::Nine)};
    auto round = table({poserHand, targetHand}, random, WildDrawFourMode::Strict, PenaltyStacking::Ladder);
    const auto first = round.apply(
        player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt, .target = std::nullopt}, random);
    REQUIRE(first.has_value());
    REQUIRE(std::holds_alternative<AwaitingStackResponse>(round.phase()));

    const auto answer = round.apply(
        player(1), PlayCard{.cardId = CardId{2000}, .chosenColor = Color::Red, .target = std::nullopt}, random);

    REQUIRE(answer.has_value());
    const auto* stack = std::get_if<AwaitingStackResponse>(&round.phase());
    REQUIRE(stack != nullptr);
    REQUIRE(stack->total == 6);
}
