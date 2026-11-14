#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

// The 7-0 house rule (ADR 0041): a 7 swaps the hand of its player with the one of a player of their choice, a 0 moves
// every hand one seat in the direction of play. Hands are told apart by the id of their first card.

using namespace uno::core;
using uno::testing::coloredCard;
using uno::testing::deckFromHands;
using uno::testing::handOf;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;

namespace {

constexpr std::uint64_t kSeed = 11;

// Seven cards, all red, with the given ranks first (the rest are Fives), ids from `firstId`.
[[nodiscard]] std::vector<Card> redHand(std::uint32_t firstId, const std::vector<Rank>& ranks = {})
{
    std::vector<Card> hand;
    hand.reserve(kHandSize);
    for (std::uint32_t slot = 0; slot < kHandSize; ++slot) {
        const Rank rank = slot < ranks.size() ? ranks.at(slot) : Rank::Five;
        hand.push_back(coloredCard(firstId + slot, Color::Red, rank));
    }
    return hand;
}

// `count` players on a red Two, seat 0 to play. `given` are the hands of the first seats, the others are red Fives from
// 1000, 2000...
[[nodiscard]] Round table(std::size_t count, const std::vector<std::vector<Card>>& given, SeededRandomSource& random,
                          bool sevenZero = true, DrawRule drawRule = DrawRule::Official)
{
    std::vector<std::vector<Card>> hands = given;
    for (std::size_t seat = given.size(); seat < count; ++seat) {
        hands.push_back(redHand(static_cast<std::uint32_t>(1000 * seat)));
    }
    return startedRound(
        {
            .seats = players(count),
            .dealer = player(count - 1),
            .deck = deckFromHands(hands, coloredCard(40, Color::Red, Rank::Two)),
            .drawRule = drawRule,
            .sevenZero = sevenZero,
        },
        random);
}

[[nodiscard]] PlayCard play(std::uint32_t id, std::optional<std::size_t> targetSeat = std::nullopt)
{
    return PlayCard{
        .cardId = CardId{id},
        .chosenColor = std::nullopt,
        .target = targetSeat.has_value() ? std::optional<PlayerId>{player(*targetSeat)} : std::nullopt,
    };
}

[[nodiscard]] CardId firstCardOf(const Round& round, std::size_t seat)
{
    return handOf(round, player(seat)).front().id;
}

constexpr std::uint32_t kSeven = 0;
constexpr std::uint32_t kZero = 0;

} // namespace

TEST_CASE("A 7 swaps the hand of its player with the chosen one, and the turn goes on", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(3, {redHand(0, {Rank::Seven})}, random);

    const auto events = round.apply(player(0), play(kSeven, 2), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{kSeven}},
                           HandsSwapped{.player = player(0), .target = player(2)},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(firstCardOf(round, 0) == CardId{2000});
    REQUIRE(handOf(round, player(0)).size() == kHandSize);
    REQUIRE(handOf(round, player(2)).size() == kHandSize - 1);
    REQUIRE(firstCardOf(round, 2) == CardId{1});
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("A 7 needs a player to swap with, who is another player of the round", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(3, {redHand(0, {Rank::Seven})}, random);
    const Round before = round;

    REQUIRE(round.apply(player(0), play(kSeven), random).error() == DomainError::SwapTargetRequired);
    REQUIRE(round.apply(player(0), play(kSeven, 0), random).error() == DomainError::SwapTargetInvalid);
    const auto stranger = PlayCard{.cardId = CardId{kSeven}, .chosenColor = std::nullopt, .target = player(9)};
    REQUIRE(round.apply(player(0), stranger, random).error() == DomainError::SwapTargetInvalid);
    REQUIRE(round == before);
}

TEST_CASE("With two players the only player to swap with is chosen for you", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(2, {redHand(0, {Rank::Seven})}, random);

    const auto events = round.apply(player(0), play(kSeven), random);

    REQUIRE(events.has_value());
    REQUIRE(firstCardOf(round, 0) == CardId{1000});
    REQUIRE(handOf(round, player(1)).size() == kHandSize - 1);
}

TEST_CASE("Without the house rule a 7 is a plain card and refuses a target", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(3, {redHand(0, {Rank::Seven})}, random, false);

    REQUIRE(round.apply(player(0), play(kSeven, 1), random).error() == DomainError::TargetNotAllowed);
    const auto events = round.apply(player(0), play(kSeven), random);

    REQUIRE(events.has_value());
    REQUIRE(firstCardOf(round, 0) == CardId{1});
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{kSeven}},
                           TurnChanged{.player = player(1)},
                       });
}

TEST_CASE("A 0 moves every hand to the next player in the direction of play", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(3, {redHand(0, {Rank::Zero})}, random);

    const auto events = round.apply(player(0), play(kZero), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{kZero}},
                           HandsRotated{.direction = Direction::Clockwise},
                           TurnChanged{.player = player(1)},
                       });
    // Seat 0 gave its six cards to seat 1, seat 1 gave its seven to seat 2, seat 2 gave its seven to seat 0
    REQUIRE(firstCardOf(round, 1) == CardId{1});
    REQUIRE(firstCardOf(round, 2) == CardId{1000});
    REQUIRE(firstCardOf(round, 0) == CardId{2000});
    REQUIRE(handOf(round, player(1)).size() == kHandSize - 1);
    requireRoundInvariants(round);
}

TEST_CASE("A 0 turns the hands against the direction when the play goes that way", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    // Seat 0 reverses (with three players the direction changes and seat 2 plays next), then seat 2 plays a 0
    auto round = table(3, {redHand(0, {Rank::Reverse}), redHand(1000), redHand(2000, {Rank::Zero})}, random);
    REQUIRE(round.apply(player(0), play(0), random).has_value());
    REQUIRE(round.direction() == Direction::CounterClockwise);
    REQUIRE(round.currentPlayer() == player(2));

    const auto events = round.apply(player(2), play(2000), random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(2), .cardId = CardId{2000}},
                           HandsRotated{.direction = Direction::CounterClockwise},
                           TurnChanged{.player = player(1)},
                       });
    // Against the direction of the seats now: seat 2 gave its cards to seat 1, seat 1 to seat 0, seat 0 to seat 2
    REQUIRE(firstCardOf(round, 1) == CardId{2001});
    REQUIRE(firstCardOf(round, 0) == CardId{1000});
    REQUIRE(firstCardOf(round, 2) == CardId{1});
    requireRoundInvariants(round);
}

TEST_CASE("With two players a 0 swaps the two hands", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(2, {redHand(0, {Rank::Zero})}, random);

    REQUIRE(round.apply(player(0), play(kZero), random).has_value());

    REQUIRE(firstCardOf(round, 0) == CardId{1000});
    REQUIRE(firstCardOf(round, 1) == CardId{1});
}

TEST_CASE("A 0 has no target", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    auto round = table(3, {redHand(0, {Rank::Zero})}, random);

    REQUIRE(round.apply(player(0), play(kZero, 1), random).error() == DomainError::TargetNotAllowed);
}

TEST_CASE("A 7 or a 0 played as the last card ends the round without exchanging anything", "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    // Six red Skips, which with two players give the turn straight back, then the last card
    const auto last = GENERATE(Rank::Seven, Rank::Zero);
    std::vector<Rank> ranks(6, Rank::Skip);
    ranks.push_back(last);
    auto round = table(2, {redHand(0, ranks)}, random);
    for (std::uint32_t id = 0; id < 6; ++id) {
        REQUIRE(round.apply(player(0), play(id), random).has_value());
    }
    REQUIRE(handOf(round, player(0)).size() == 1);

    const auto events = round.apply(player(0), play(6), random);

    REQUIRE(events.has_value());
    for (const auto& event : *events) {
        REQUIRE_FALSE(std::holds_alternative<HandsSwapped>(event));
        REQUIRE_FALSE(std::holds_alternative<HandsRotated>(event));
    }
    REQUIRE(std::holds_alternative<RoundOver>(round.phase()));
    REQUIRE(handOf(round, player(0)).empty());
    requireRoundInvariants(round);
}

TEST_CASE("After an exchange nobody keeps an announcement or a UNO window for a hand they no longer hold",
          "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    // Five Skips (the turn comes back), a 7 and a Five: playing the 7 leaves the player with one card, until they swap
    std::vector<Rank> ranks(5, Rank::Skip);
    ranks.push_back(Rank::Seven);
    auto round = table(2, {redHand(0, ranks)}, random);
    for (std::uint32_t id = 0; id < 5; ++id) {
        REQUIRE(round.apply(player(0), play(id), random).has_value());
    }

    REQUIRE(round.apply(player(0), play(5), random).has_value());

    REQUIRE(handOf(round, player(0)).size() == kHandSize);
    REQUIRE(handOf(round, player(1)).size() == 1);
    REQUIRE(round.unoWindows().empty());
    REQUIRE_FALSE(round.hasCalledUno(player(0)));
    REQUIRE_FALSE(round.hasCalledUno(player(1)));
    requireRoundInvariants(round);
}

TEST_CASE("In the guided draw a 7 or a 0 is a special card: it is played by choice, never for the player",
          "[core][round][sevenZero]")
{
    SeededRandomSource random{kSeed};
    // The 7 is the only playable card: the other cards are blue Fives, on a red Two
    std::vector<Card> hand{coloredCard(0, Color::Red, Rank::Seven)};
    for (std::uint32_t id = 1; id < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Blue, Rank::Five));
    }
    const auto withRule = table(3, {hand}, random, true, DrawRule::Guided);
    const auto withoutRule = table(3, {hand}, random, false, DrawRule::Guided);

    REQUIRE(withRule.isSpecial(coloredCard(0, Color::Red, Rank::Seven)));
    REQUIRE(withRule.canDraw(player(0)));
    REQUIRE_FALSE(withoutRule.isSpecial(coloredCard(0, Color::Red, Rank::Seven)));
    REQUIRE_FALSE(withoutRule.canDraw(player(0)));
}
