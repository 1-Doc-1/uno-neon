#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

using uno::core::AwaitingPenaltyResponse;
using uno::core::Card;
using uno::core::CardId;
using uno::core::CardPlayed;
using uno::core::ChallengeResolved;
using uno::core::ChooseColor;
using uno::core::Color;
using uno::core::ColorChosen;
using uno::core::DeckReshuffled;
using uno::core::DomainError;
using uno::core::DomainEvent;
using uno::core::DrawCard;
using uno::core::kHandSize;
using uno::core::Pass;
using uno::core::PenaltyCardsDrawn;
using uno::core::PenaltyResponse;
using uno::core::PlayCard;
using uno::core::PlayerSkipped;
using uno::core::Rank;
using uno::core::RespondPenalty;
using uno::core::TurnChanged;
using uno::testing::coloredCard;
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

// A 7-card hand (ids 0..6): `first` as given, the rest filled with red Fives.
[[nodiscard]] std::vector<Card> handWith(Card first)
{
    auto hand = plainCards(kHandSize);
    hand.front() = first;
    return hand;
}

// Same, but with a second card (id 1) also given — used to build a Wild Draw Four hand that still
// holds a card of the current color, making the play illegal.
[[nodiscard]] std::vector<Card> handWith(Card first, Card second)
{
    auto hand = handWith(first);
    hand.at(1) = second;
    return hand;
}

// Same id-range convention as round_effects_test.cpp's deckFor: `currentHand` uses ids 0..6, the
// filler for every other seat uses 100+, `top`/`afterTop` are expected to use ids from 40 up.
[[nodiscard]] std::vector<Card> deckFor(std::size_t playerCount, const std::vector<Card>& currentHand, const Card& top,
                                        const std::vector<Card>& afterTop = {})
{
    std::vector<Card> deck;
    deck.reserve((playerCount * kHandSize) + 1 + afterTop.size());
    for (std::size_t i = 0; i < kHandSize; ++i) {
        deck.push_back(currentHand.at(i));
        for (std::size_t seat = 1; seat < playerCount; ++seat) {
            const auto id = 100 + static_cast<std::uint32_t>((seat * kHandSize) + i);
            deck.push_back(coloredCard(id, Color::Red, Rank::Five));
        }
    }
    deck.push_back(top);
    std::ranges::copy(afterTop, std::back_inserter(deck));
    return deck;
}

} // namespace

TEST_CASE("Playing WildDrawFour without a chosen color is rejected", "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);

    const auto result = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = std::nullopt}, random);

    REQUIRE(result.error() == DomainError::ColorRequired);
}

TEST_CASE("Playing WildDrawFour moves the turn to the targeted player", "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);

    const auto events = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events == std::vector<DomainEvent>{
                           CardPlayed{.player = player(0), .cardId = CardId{0}},
                           ColorChosen{.player = player(0), .color = Color::Green},
                           TurnChanged{.player = player(1)},
                       });
    REQUIRE(round.currentPlayer() == player(1));
    REQUIRE(round.currentColor() == Color::Green);
    const auto* awaiting = std::get_if<AwaitingPenaltyResponse>(&round.phase());
    REQUIRE(awaiting != nullptr);
    REQUIRE(awaiting->wildDrawFourPlayer == player(0));
    requireRoundInvariants(round);
}

TEST_CASE("Other actions are rejected while awaiting a penalty response", "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());

    REQUIRE(round.apply(player(1), DrawCard{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(round.apply(player(1), Pass{}, random).error() == DomainError::InvalidPhase);
    REQUIRE(round.apply(player(1), ChooseColor{.color = Color::Red}, random).error() == DomainError::InvalidPhase);
}

TEST_CASE("Responding to a penalty from outside the awaiting phase is rejected", "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound(
        {.seats = players(2), .dealer = player(1), .deck = deckFor(2, plainCards(kHandSize), top)}, random);

    const auto result = round.apply(player(0), RespondPenalty{.response = PenaltyResponse::Accept}, random);

    REQUIRE(result.error() == DomainError::InvalidPhase);
}

TEST_CASE("Responding to a penalty is rejected from anyone but the targeted player", "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    auto round = startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());

    const auto fromPoser = round.apply(player(0), RespondPenalty{.response = PenaltyResponse::Accept}, random);
    REQUIRE(fromPoser.error() == DomainError::NotYourTurn);
    const auto fromBystander = round.apply(player(2), RespondPenalty{.response = PenaltyResponse::Accept}, random);
    REQUIRE(fromBystander.error() == DomainError::NotYourTurn);
}

TEST_CASE("Accepting a Wild Draw Four penalty makes the target draw four and lose their turn",
          "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),
        coloredCard(42, Color::Green, Rank::Two),
        coloredCard(43, Color::Green, Rank::Three),
        coloredCard(44, Color::Green, Rank::Four),
    };
    auto round =
        startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top, afterTop)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());

    const auto events = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Accept}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events ==
            std::vector<DomainEvent>{
                PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}}},
                PlayerSkipped{.skippedPlayer = player(1)},
                TurnChanged{.player = player(2)},
            });
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 4);
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("Challenging a bluffed Wild Draw Four makes the poser draw four and the challenger play on",
          "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    // player(0) keeps a Blue card (the current color) besides the +4: the play is illegal.
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour), coloredCard(1, Color::Blue, Rank::Six));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),
        coloredCard(42, Color::Green, Rank::Two),
        coloredCard(43, Color::Green, Rank::Three),
        coloredCard(44, Color::Green, Rank::Four),
    };
    auto round =
        startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top, afterTop)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());
    const std::vector<Card> poserHandAfterPlaying{handOf(round, player(0)).begin(), handOf(round, player(0)).end()};

    const auto events = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events ==
            std::vector<DomainEvent>{
                ChallengeResolved{.challenger = player(1),
                                  .challenged = player(0),
                                  .wasBluff = true,
                                  .penalizedPlayer = player(0),
                                  .penaltyAmount = 4,
                                  .revealedHand = poserHandAfterPlaying},
                PenaltyCardsDrawn{.player = player(0), .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}}},
            });
    REQUIRE(handOf(round, player(0)).size() == kHandSize - 1 + 4);
    // The challenger keeps the turn they already had: nobody was skipped, nothing advanced.
    REQUIRE(round.currentPlayer() == player(1));
    requireRoundInvariants(round);
}

TEST_CASE("Challenging a legal Wild Draw Four makes the challenger draw six and lose their turn",
          "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),   coloredCard(42, Color::Green, Rank::Two),
        coloredCard(43, Color::Green, Rank::Three), coloredCard(44, Color::Green, Rank::Four),
        coloredCard(45, Color::Green, Rank::Five),  coloredCard(46, Color::Green, Rank::Six),
    };
    auto round =
        startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top, afterTop)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());
    const std::vector<Card> poserHandAfterPlaying{handOf(round, player(0)).begin(), handOf(round, player(0)).end()};

    const auto events = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random);

    REQUIRE(events.has_value());
    REQUIRE(*events ==
            std::vector<DomainEvent>{
                ChallengeResolved{.challenger = player(1),
                                  .challenged = player(0),
                                  .wasBluff = false,
                                  .penalizedPlayer = player(1),
                                  .penaltyAmount = 6,
                                  .revealedHand = poserHandAfterPlaying},
                PenaltyCardsDrawn{.player = player(1),
                                  .cards = {CardId{41}, CardId{42}, CardId{43}, CardId{44}, CardId{45}, CardId{46}}},
                PlayerSkipped{.skippedPlayer = player(1)},
                TurnChanged{.player = player(2)},
            });
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 6);
    REQUIRE(round.currentPlayer() == player(2));
    requireRoundInvariants(round);
}

TEST_CASE("A failed challenge still draws only as many cards as the piles can provide", "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    const auto hand = handWith(wildCard(0, Rank::WildDrawFour));
    const auto top = coloredCard(40, Color::Blue, Rank::Two);
    // Only two cards left to draw, and nothing else to reshuffle once the +4 becomes the new top:
    // the 6-card penalty can only ever yield 3 cards (the 2 left, plus the old top once reshuffled).
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),
        coloredCard(42, Color::Green, Rank::Two),
    };
    auto round =
        startedRound({.seats = players(2), .dealer = player(1), .deck = deckFor(2, hand, top, afterTop)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Green}, random);
    REQUIRE(played.has_value());
    const std::vector<Card> poserHandAfterPlaying{handOf(round, player(0)).begin(), handOf(round, player(0)).end()};

    const auto events = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random);

    REQUIRE(events.has_value());
    // The draw pile only had 2 cards left, and reshuffling the discard pile (everything but its
    // new +4 top) only ever yields the one Blue Two beneath it: 3 cards total, not the nominal 6.
    REQUIRE(*events == std::vector<DomainEvent>{
                           ChallengeResolved{.challenger = player(1),
                                             .challenged = player(0),
                                             .wasBluff = false,
                                             .penalizedPlayer = player(1),
                                             .penaltyAmount = 6,
                                             .revealedHand = poserHandAfterPlaying},
                           DeckReshuffled{},
                           PenaltyCardsDrawn{.player = player(1), .cards = {CardId{41}, CardId{42}, CardId{40}}},
                           PlayerSkipped{.skippedPlayer = player(1)},
                           TurnChanged{.player = player(0)},
                       });
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 3);
    REQUIRE(round.currentPlayer() == player(0)); // only 2 players: skipping the target returns here
    requireRoundInvariants(round);
}

TEST_CASE("A Wild Draw Four is judged against the color current when it was played, not the chosen one",
          "[core][round][wildDrawFour]")
{
    SeededRandomSource random{kSeed};
    // Current color is Red and player(0) holds no Red: the +4 is legal, even though they keep Blue
    // cards and choose Blue (judging against the newly chosen color would wrongly call it a bluff).
    std::vector<Card> hand{wildCard(0, Rank::WildDrawFour)};
    for (std::uint32_t id = 1; id < kHandSize; ++id) {
        hand.push_back(coloredCard(id, Color::Blue, Rank::Six));
    }
    const auto top = coloredCard(40, Color::Red, Rank::Two);
    const std::vector<Card> afterTop{
        coloredCard(41, Color::Green, Rank::One),   coloredCard(42, Color::Green, Rank::Two),
        coloredCard(43, Color::Green, Rank::Three), coloredCard(44, Color::Green, Rank::Four),
        coloredCard(45, Color::Green, Rank::Five),  coloredCard(46, Color::Green, Rank::Six),
    };
    auto round =
        startedRound({.seats = players(3), .dealer = player(2), .deck = deckFor(3, hand, top, afterTop)}, random);
    const auto played = round.apply(player(0), PlayCard{.cardId = CardId{0}, .chosenColor = Color::Blue}, random);
    REQUIRE(played.has_value());

    const auto events = round.apply(player(1), RespondPenalty{.response = PenaltyResponse::Challenge}, random);

    REQUIRE(events.has_value());
    const auto* verdict = std::get_if<ChallengeResolved>(&events->front());
    REQUIRE(verdict != nullptr);
    REQUIRE_FALSE(verdict->wasBluff);
    REQUIRE(verdict->penalizedPlayer == player(1));
    REQUIRE(handOf(round, player(1)).size() == kHandSize + 6);
}
