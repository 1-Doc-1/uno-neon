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

using uno::core::AwaitingPenaltyResponse;
using uno::core::CallUno;
using uno::core::Card;
using uno::core::CardId;
using uno::core::CatchUno;
using uno::core::Color;
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
using uno::core::UnoCalled;
using uno::core::UnoCaught;
using uno::testing::coloredCard;
using uno::testing::deckGivingFirstHand;
using uno::testing::handOf;
using uno::testing::plainCards;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

namespace {

constexpr std::uint64_t kSeed = 42;
constexpr std::uint32_t kSecondToLastId = 5;
constexpr std::uint32_t kLastCardId = 6;
constexpr std::uint32_t kPlayerOneRedFiveId = 107;

void play(Round& round, SeededRandomSource& random, std::uint32_t cardId, std::optional<Color> color = std::nullopt)
{
    const auto result =
        round.apply(round.currentPlayer(), PlayCard{.cardId = CardId{cardId}, .chosenColor = color}, random);
    REQUIRE(result.has_value());
}

[[nodiscard]] std::vector<Card> greenSevensFollowing()
{
    std::vector<Card> cards;
    for (std::uint32_t id = 41; id <= 46; ++id) {
        cards.push_back(coloredCard(id, Color::Green, Rank::Seven));
    }
    return cards;
}

// Two players; red Two is flipped, so the first player (player-0) starts. Their hand is five red
// Skips (ids 0-4, each hands the turn straight back with two players), then `secondToLast` (id 5)
// and a red Five (id 6). After the five Skips, player-0 is to play again with exactly two cards.
// player-1 holds red Fives (ids 100+); `afterTop` follows in the draw pile.
[[nodiscard]] Round roundWithTwoCardsLeft(const Card& secondToLast, const std::vector<Card>& afterTop,
                                          SeededRandomSource& random)
{
    std::vector<Card> hand;
    hand.reserve(kHandSize);
    for (std::uint32_t id = 0; id < kSecondToLastId; ++id) {
        hand.push_back(coloredCard(id, Color::Red, Rank::Skip));
    }
    hand.push_back(secondToLast);
    hand.push_back(coloredCard(kLastCardId, Color::Red, Rank::Five));
    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, hand, coloredCard(40, Color::Red, Rank::Two), afterTop),
        },
        random);
    for (std::uint32_t id = 0; id < kSecondToLastId; ++id) {
        play(round, random, id);
    }
    REQUIRE(handOf(round, player(0)).size() == 2);
    return round;
}

[[nodiscard]] Round roundWithTwoCardsLeft(SeededRandomSource& random)
{
    return roundWithTwoCardsLeft(coloredCard(kSecondToLastId, Color::Red, Rank::Skip), greenSevensFollowing(), random);
}

// player-0 leaves themselves with one card (the one with id 5) by playing the red Five.
void leaveFirstPlayerWithOneCard(Round& round, SeededRandomSource& random)
{
    play(round, random, kLastCardId);
    REQUIRE(handOf(round, player(0)).size() == 1);
}

} // namespace

TEST_CASE("Playing the second-to-last card without announcing UNO opens the UNO window", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);

    leaveFirstPlayerWithOneCard(round, random);

    REQUIRE(round.hasUnoWindowOn(player(0)));
    REQUIRE(!round.hasCalledUno(player(0)));
    requireRoundInvariants(round);
}

TEST_CASE("Announcing UNO before playing the second-to-last card opens no window", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);

    const auto events = round.apply(player(0), CallUno{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{UnoCalled{.player = player(0)}});
    leaveFirstPlayerWithOneCard(round, random);
    REQUIRE(round.unoWindows().empty());
    REQUIRE(round.hasCalledUno(player(0)));
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::UnoWindowClosed);
    requireRoundInvariants(round);
}

TEST_CASE("Announcing UNO in the window closes it", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);

    const auto events = round.apply(player(0), CallUno{}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{UnoCalled{.player = player(0)}});
    REQUIRE(round.unoWindows().empty());
    REQUIRE(round.hasCalledUno(player(0)));
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::UnoWindowClosed);
    requireRoundInvariants(round);
}

TEST_CASE("Catching UNO makes the offender draw two cards and closes the window", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);

    const auto events = round.apply(player(1), CatchUno{.target = player(0)}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           UnoCaught{.catcher = player(1), .target = player(0)},
                           PenaltyCardsDrawn{.player = player(0), .cards = {CardId{41}, CardId{42}}},
                       });
    REQUIRE(handOf(round, player(0)).size() == 3);
    REQUIRE(round.unoWindows().empty());
    // The first catch wins: a second, simultaneous one finds the window closed (SPEC §5).
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::UnoWindowClosed);
    requireRoundInvariants(round);
}

TEST_CASE("A rejected action does not close the UNO window", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);

    REQUIRE(round.apply(player(1), Pass{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(round.apply(player(1), PlayCard{.cardId = CardId{999}, .chosenColor = std::nullopt}, random).error() ==
            DomainError::CardNotInHand);
    REQUIRE(round.apply(player(0), DrawCard{}, random).error() == DomainError::NotYourTurn);

    REQUIRE(round.hasUnoWindowOn(player(0)));
}

TEST_CASE("A Skip left as the last-but-one play keeps the window open for the other player", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    // With two players, the Skip hands the turn straight back to player-0, who is the offender.
    auto round = roundWithTwoCardsLeft(random);
    play(round, random, kSecondToLastId);

    REQUIRE(round.currentPlayer() == player(0));
    REQUIRE(round.hasUnoWindowOn(player(0)));
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).has_value());
    requireRoundInvariants(round);
}

TEST_CASE("Invalid catches are rejected", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);

    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::UnoWindowClosed);

    leaveFirstPlayerWithOneCard(round, random);
    REQUIRE(round.apply(player(0), CatchUno{.target = player(0)}, random).error() == DomainError::CannotCatchSelf);
    REQUIRE(round.apply(player(1), CatchUno{.target = player(1)}, random).error() == DomainError::UnoWindowClosed);
    REQUIRE(round.apply(player(7), CatchUno{.target = player(0)}, random).error() == DomainError::UnknownPlayer);
    REQUIRE(round.hasUnoWindowOn(player(0)));
}

TEST_CASE("Announcing UNO when it is not due has no effect", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = startedRound(
        {
            .seats = players(2),
            .dealer = player(1),
            .deck = deckGivingFirstHand(2, plainCards(7), coloredCard(40, Color::Red, Rank::Two)),
        },
        random);
    const auto before = round;

    // Seven cards in hand, whether it is their turn or not.
    const auto fromCurrent = round.apply(player(0), CallUno{}, random);
    const auto fromOther = round.apply(player(1), CallUno{}, random);

    REQUIRE(fromCurrent.has_value());
    REQUIRE(fromCurrent->empty());
    REQUIRE(fromOther.has_value());
    REQUIRE(fromOther->empty());
    REQUIRE(round == before);
}

TEST_CASE("Announcing UNO twice only counts once", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    REQUIRE(round.apply(player(0), CallUno{}, random).has_value());

    const auto again = round.apply(player(0), CallUno{}, random);

    REQUIRE(again.has_value());
    REQUIRE(again->empty());
}

TEST_CASE("Announcing UNO never saves someone else", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);

    // player-1 cannot save player-0: a call only ever concerns the caller.
    const auto fromOther = round.apply(player(1), CallUno{}, random);

    REQUIRE(fromOther.has_value());
    REQUIRE(fromOther->empty());
    REQUIRE(round.hasUnoWindowOn(player(0)));
}

TEST_CASE("Unknown players cannot announce UNO", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);

    REQUIRE(round.apply(player(7), CallUno{}, random).error() == DomainError::UnknownPlayer);
}

TEST_CASE("An announcement is lost as soon as the player draws cards", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    REQUIRE(round.apply(player(0), CallUno{}, random).has_value());

    // The green Seven drawn is not playable on the red Two: the turn ends, with three cards in hand.
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());

    REQUIRE(!round.hasCalledUno(player(0)));
    requireRoundInvariants(round);
}

TEST_CASE("An announcement made before drawing a playable card does not outlive a pass", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    // After player-0 plays the red Five, player-1 draws a green Seven and passes the turn back;
    // player-0 then draws a red Seven (playable): two cards, a decision to take.
    auto round = roundWithTwoCardsLeft(coloredCard(kSecondToLastId, Color::Red, Rank::Skip),
                                       {
                                           coloredCard(41, Color::Green, Rank::Seven),
                                           coloredCard(42, Color::Red, Rank::Seven),
                                       },
                                       random);
    leaveFirstPlayerWithOneCard(round, random);
    REQUIRE(round.apply(player(1), DrawCard{}, random).has_value());
    REQUIRE(round.apply(player(0), DrawCard{}, random).has_value());
    REQUIRE(handOf(round, player(0)).size() == 2);
    REQUIRE(round.apply(player(0), CallUno{}, random).has_value());
    REQUIRE(round.hasCalledUno(player(0)));

    REQUIRE(round.apply(player(0), Pass{}, random).has_value());

    REQUIRE(!round.hasCalledUno(player(0)));
}

TEST_CASE("The offender can be caught while a Wild Draw Four penalty is pending", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(wildCard(kSecondToLastId, Rank::WildDrawFour), greenSevensFollowing(), random);
    play(round, random, kSecondToLastId, Color::Red);
    REQUIRE(round.hasUnoWindowOn(player(0)));

    const auto events = round.apply(player(1), CatchUno{.target = player(0)}, random);

    REQUIRE(events.has_value());
    REQUIRE(std::holds_alternative<AwaitingPenaltyResponse>(round.phase()));
    requireRoundInvariants(round);
}

TEST_CASE("A lost challenge closes the window: the offender no longer holds one card", "[core][round][uno]")
{
    SeededRandomSource random{kSeed};
    // The +4 is a bluff (player-0 keeps the red Five), so the challenge makes player-0 draw four.
    auto round = roundWithTwoCardsLeft(wildCard(kSecondToLastId, Rank::WildDrawFour), greenSevensFollowing(), random);
    play(round, random, kSecondToLastId, Color::Red);

    REQUIRE(round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random).has_value());

    REQUIRE(round.unoWindows().empty());
    REQUIRE(handOf(round, player(0)).size() == 5);
    requireRoundInvariants(round);
}

// G1 (bug): the window used to die as soon as the next player played or drew, so only the very next player could
// ever catch the offender, and only before acting. Anybody must be able to, while the window lives (ADR 0018).
TEST_CASE("The offender can still be caught after the next player has played", "[core][round][uno][window]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);
    play(round, random, kPlayerOneRedFiveId); // player-1 acts; it is player-0's turn again

    const auto events = round.apply(player(1), CatchUno{.target = player(0)}, random);

    REQUIRE(events.has_value());
    REQUIRE(handOf(round, player(0)).size() == 3);
    requireRoundInvariants(round);
}

TEST_CASE("The offender can still be caught after the next player has drawn", "[core][round][uno][window]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);
    REQUIRE(round.apply(player(1), DrawCard{}, random).has_value());

    REQUIRE(round.hasUnoWindowOn(player(0)));
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).has_value());
}

TEST_CASE("Announcing UNO at one card is still accepted once the window has run out of time",
          "[core][round][uno][window]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);
    round.closeUnoWindow(player(0));

    REQUIRE(round.unoWindows().empty());
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::UnoWindowClosed);
    REQUIRE(round.canCallUno(player(0)));
    const auto events = round.apply(player(0), CallUno{}, random);
    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{UnoCalled{.player = player(0)}});
    requireRoundInvariants(round);
}

TEST_CASE("A player caught once cannot be caught again until they are back to one card", "[core][round][uno][window]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);
    leaveFirstPlayerWithOneCard(round, random);
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).has_value());

    REQUIRE(round.unoWindows().empty());
    REQUIRE(round.apply(player(1), CatchUno{.target = player(0)}, random).error() == DomainError::UnoWindowClosed);
}

TEST_CASE("Closing a window nobody has opened does nothing", "[core][round][uno][window]")
{
    SeededRandomSource random{kSeed};
    auto round = roundWithTwoCardsLeft(random);

    round.closeUnoWindow(player(0));
    round.closeUnoWindow(player(7));

    REQUIRE(round.unoWindows().empty());
    requireRoundInvariants(round);
}
