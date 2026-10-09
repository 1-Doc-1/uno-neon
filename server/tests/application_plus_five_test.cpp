#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/card.hpp"
#include "uno/core/deck.hpp"
#include "uno/core/match.hpp"
#include "uno/core/turn_phase.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

// The Wild Draw Five through the application (ADR 0028): the settings that put it in the deck, its target on the wire,
// the pace of what it causes, and what the server does for a target who does not answer.

using namespace std::chrono_literals;

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::refusal;
using uno::testing::refusalReason;
using uno::testing::Table;
using uno::testing::toRequest;
using uno::testing::withoutPresentationDelay;

// A deck rich in Wild Draw Five, so that the player on turn often holds one.
RoomSettingsPatch richInPlusFive()
{
    RoomSettingsPatch patch;
    patch.wildDrawFiveMultiplier = core::CardMultiplier::Five;
    return patch;
}

[[nodiscard]] std::optional<core::Card> plusFiveOf(const core::Round& round, const core::PlayerId& who)
{
    const auto hand = round.hand(who).value_or(std::span<const core::Card>{});
    const auto found = std::ranges::find(hand, core::Rank::WildDrawFive, &core::Card::rank);
    return found == hand.end() ? std::nullopt : std::optional<core::Card>(*found);
}

struct PlusFiveTable {
    std::unique_ptr<Table> table;
    core::Card card;
};

// `players` players, drawRule as given, with the player on turn holding a Wild Draw Five. `targetHolds` asks for the
// seat after theirs to hold one too (or not to).
PlusFiveTable tableWherePlayerOnTurnHoldsPlusFive(std::size_t players, core::DrawRule drawRule,
                                                  std::optional<bool> targetHolds, Timeouts timeouts)
{
    for (std::uint64_t seed = 1; seed < 400; ++seed) {
        auto table = std::make_unique<Table>(players, seed, core::MatchLength::SingleRound, drawRule, false,
                                             core::DrawAmount::One, timeouts);
        table->players.front().send(request::UpdateSettings{.settings = richInPlusFive()});
        table->start();
        const auto& round = table->room().match->round();
        const auto card = plusFiveOf(round, round.currentPlayer());
        if (!card.has_value() || !std::holds_alternative<core::AwaitingPlay>(round.phase())) {
            continue;
        }
        const auto& seats = round.seats();
        const auto self = std::ranges::find(seats, round.currentPlayer()) - seats.begin();
        const auto& target = *std::next(seats.begin(), (self + 1) % static_cast<std::ptrdiff_t>(seats.size()));
        if (targetHolds.has_value() && plusFiveOf(round, target).has_value() != *targetHolds) {
            continue;
        }
        table->harness.clock.advanceMillis(60'000);
        table->clearInboxes();
        return {.table = std::move(table), .card = *card};
    }
    throw std::logic_error{"no seed gave the player on turn a Wild Draw Five"};
}

core::PlayerId playerAfter(const core::Round& round, const core::PlayerId& who)
{
    const auto& seats = round.seats();
    const auto self = static_cast<std::size_t>(std::ranges::find(seats, who) - seats.begin());
    return *std::next(seats.begin(), static_cast<std::ptrdiff_t>((self + 1) % seats.size()));
}

} // namespace

TEST_CASE("The host sets how many Draw Two, Wild Draw Four and Wild Draw Five the deck holds",
          "[app][plusFive][settings]")
{
    Table table(3, 7, core::MatchLength::SingleRound);
    RoomSettingsPatch patch;
    patch.drawTwoMultiplier = core::CardMultiplier::Two;
    patch.wildDrawFourMultiplier = core::CardMultiplier::Three;
    patch.wildDrawFiveMultiplier = core::CardMultiplier::Five;

    table.players.front().send(request::UpdateSettings{.settings = patch});
    table.start();

    const auto& settings = table.room().settings;
    REQUIRE(settings.drawTwoMultiplier == core::CardMultiplier::Two);
    REQUIRE(settings.wildDrawFourMultiplier == core::CardMultiplier::Three);
    REQUIRE(settings.wildDrawFiveMultiplier == core::CardMultiplier::Five);
    // The match is dealt with that deck: every card is somewhere, none is missing
    const auto& round = table.room().match->round();
    std::size_t cards = round.drawPile().size() + round.discardPile().size();
    for (const auto& seated : round.seats()) {
        cards += round.hand(seated)->size();
    }
    REQUIRE(cards == core::compositionOf(settings.deck()).total());
}

TEST_CASE("Only the host changes the composition of the deck", "[app][plusFive][settings]")
{
    Table table(3, 7, core::MatchLength::SingleRound);

    table.players.at(1).send(request::UpdateSettings{.settings = richInPlusFive()});

    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);
    REQUIRE(table.room().settings.wildDrawFiveMultiplier == core::CardMultiplier::One);
}

TEST_CASE("The deck of a standard room holds one Wild Draw Five pair", "[app][plusFive][settings]")
{
    Table table(2, 7, core::MatchLength::SingleRound);
    table.start();

    const auto& round = table.room().match->round();
    REQUIRE(round.drawPile().size() + round.discardPile().size() + (2 * core::kHandSize) == core::kStandardDeckSize);
}

TEST_CASE("A Wild Draw Five without a valid target is refused with its reason, and nothing changes", "[app][plusFive]")
{
    auto [table, card] =
        tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Official, std::nullopt, withoutPresentationDelay());
    auto& actor = table->currentPlayer();
    const auto version = table->room().stateVersion;
    const auto plain = std::ranges::find_if(*table->room().match->round().hand(actor.id()),
                                            [](const core::Card& other) { return !core::isWild(other.rank); });

    SECTION("no target")
    {
        actor.send(request::PlayCard{.cardId = card.id,
                                     .chosenColor = core::Color::Red,
                                     .swapTargetId = std::nullopt,
                                     .targetId = std::nullopt});
        REQUIRE(refusalReason(actor.received()) == IllegalMoveReason::TargetRequired);
    }
    SECTION("oneself")
    {
        actor.send(request::PlayCard{
            .cardId = card.id, .chosenColor = core::Color::Red, .swapTargetId = std::nullopt, .targetId = actor.id()});
        REQUIRE(refusalReason(actor.received()) == IllegalMoveReason::InvalidTarget);
    }
    SECTION("somebody who is not in the room")
    {
        actor.send(request::PlayCard{.cardId = card.id,
                                     .chosenColor = core::Color::Red,
                                     .swapTargetId = std::nullopt,
                                     .targetId = core::PlayerId{"nobody"}});
        REQUIRE(refusalReason(actor.received()) == IllegalMoveReason::InvalidTarget);
    }
    SECTION("a target on a card that has none")
    {
        REQUIRE(plain != table->room().match->round().hand(actor.id())->end());
        actor.send(request::PlayCard{.cardId = plain->id,
                                     .chosenColor = std::nullopt,
                                     .swapTargetId = std::nullopt,
                                     .targetId = playerAfter(table->room().match->round(), actor.id())});
        REQUIRE(refusalReason(actor.received()) == IllegalMoveReason::TargetNotAllowed);
    }
    REQUIRE(table->room().stateVersion == version);
}

TEST_CASE("A played Wild Draw Five tells everybody who is targeted and how much, and who must answer",
          "[app][plusFive]")
{
    auto [table, card] =
        tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Official, std::nullopt, withoutPresentationDelay());
    const auto poser = table->room().match->round().currentPlayer();
    const auto target = playerAfter(table->room().match->round(), poser);
    auto& actor = table->playerWithId(poser);

    actor.send(request::PlayCard{
        .cardId = card.id, .chosenColor = core::Color::Green, .swapTargetId = std::nullopt, .targetId = target});

    for (auto& player : table->players) {
        const auto update = player.last<response::GameUpdate>();
        REQUIRE(update.has_value());
        const auto targeted = std::ranges::find_if(update->events, [](const core::ClientEvent& event) {
            return std::holds_alternative<core::PlusFiveTargetedEvent>(event);
        });
        REQUIRE(targeted != update->events.end());
        REQUIRE(std::get<core::PlusFiveTargetedEvent>(*targeted) ==
                core::PlusFiveTargetedEvent{.playerId = poser, .targetId = target, .total = 5});
        REQUIRE(update->view.game.currentPlayerId == target);
        REQUIRE(update->view.game.pendingDraw == 5);
    }
}

TEST_CASE("A Wild Draw Five costs the card, the colour wheel and the +5, then the pause",
          "[app][plusFive][presentation]")
{
    auto [table, card] = tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Official, std::nullopt, Timeouts{});
    const auto poser = table->room().match->round().currentPlayer();
    const auto playedAt = table->harness.clock.nowMillis();

    table->playerWithId(poser).send(request::PlayCard{.cardId = card.id,
                                                      .chosenColor = core::Color::Green,
                                                      .swapTargetId = std::nullopt,
                                                      .targetId = playerAfter(table->room().match->round(), poser)});

    REQUIRE(table->room().actionsOpenAt == playedAt + (1100ms + 2 * 1500ms).count());
}

TEST_CASE("Guided draw: a target without a Wild Draw Five draws the total by itself, one card at a time",
          "[app][plusFive][pace]")
{
    auto [table, card] = tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Guided, false, Timeouts{});
    const auto poser = table->room().match->round().currentPlayer();
    const auto target = playerAfter(table->room().match->round(), poser);
    table->playerWithId(poser).send(request::PlayCard{
        .cardId = card.id, .chosenColor = core::Color::Green, .swapTargetId = std::nullopt, .targetId = target});
    const auto version = table->room().stateVersion;
    const auto cardsBefore = table->room().match->round().hand(target)->size();
    const auto opensAt = table->room().actionsOpenAt;
    const auto now = table->harness.clock.nowMillis();

    // Nothing happens while the effect is shown, then the forced move waits its own pause
    table->harness.scheduler.advance(std::chrono::milliseconds(opensAt - now) + 1199ms);
    REQUIRE(table->room().stateVersion == version);
    table->harness.scheduler.advance(1ms);

    REQUIRE(table->room().stateVersion == version + 1);
    REQUIRE(table->room().match->round().hand(target)->size() == cardsBefore + 5);
    // The five cards are shown one by one: nobody acts before they have all arrived
    REQUIRE(table->room().actionsOpenAt >= table->harness.clock.nowMillis() + (5 * 1000ms).count());
}

TEST_CASE("Guided draw: a target holding a Wild Draw Five is left to choose", "[app][plusFive]")
{
    auto [table, card] = tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Guided, true, Timeouts{});
    const auto poser = table->room().match->round().currentPlayer();
    const auto target = playerAfter(table->room().match->round(), poser);
    table->playerWithId(poser).send(request::PlayCard{
        .cardId = card.id, .chosenColor = core::Color::Green, .swapTargetId = std::nullopt, .targetId = target});
    const auto version = table->room().stateVersion;

    table->harness.scheduler.advance(30s); // longer than the effect and the forced-move pause, shorter than the turn

    REQUIRE(table->room().stateVersion == version);
    REQUIRE(std::holds_alternative<core::AwaitingPlusFiveResponse>(table->room().match->round().phase()));
}

TEST_CASE("A target who lets the turn timer expire accepts the penalty", "[app][plusFive]")
{
    auto [table, card] = tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Official, std::nullopt, Timeouts{});
    const auto poser = table->room().match->round().currentPlayer();
    const auto target = playerAfter(table->room().match->round(), poser);
    table->playerWithId(poser).send(request::PlayCard{
        .cardId = card.id, .chosenColor = core::Color::Green, .swapTargetId = std::nullopt, .targetId = target});
    const auto cardsBefore = table->room().match->round().hand(target)->size();

    table->harness.scheduler.advance(2min);

    REQUIRE_FALSE(std::holds_alternative<core::AwaitingPlusFiveResponse>(table->room().match->round().phase()));
    REQUIRE(table->room().match->round().hand(target)->size() >= cardsBefore + 5 - 1);
}

TEST_CASE("The target answers with a Wild Draw Five through the application, and the total grows", "[app][plusFive]")
{
    auto [table, card] =
        tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Official, true, withoutPresentationDelay());
    const auto poser = table->room().match->round().currentPlayer();
    const auto target = playerAfter(table->room().match->round(), poser);
    table->playerWithId(poser).send(request::PlayCard{
        .cardId = card.id, .chosenColor = core::Color::Green, .swapTargetId = std::nullopt, .targetId = target});
    const auto answer = plusFiveOf(table->room().match->round(), target);
    REQUIRE(answer.has_value());
    table->clearInboxes();

    table->playerWithId(target).send(request::PlayCard{
        .cardId = answer->id, .chosenColor = core::Color::Blue, .swapTargetId = std::nullopt, .targetId = poser});

    const auto update = table->playerWithId(poser).last<response::GameUpdate>();
    REQUIRE(update.has_value());
    REQUIRE(update->view.game.pendingDraw == 10);
    REQUIRE(update->view.game.currentPlayerId == poser);
}

TEST_CASE("The target cannot challenge a Wild Draw Five", "[app][plusFive]")
{
    auto [table, card] =
        tableWherePlayerOnTurnHoldsPlusFive(3, core::DrawRule::Official, std::nullopt, withoutPresentationDelay());
    const auto poser = table->room().match->round().currentPlayer();
    const auto target = playerAfter(table->room().match->round(), poser);
    table->playerWithId(poser).send(request::PlayCard{
        .cardId = card.id, .chosenColor = core::Color::Green, .swapTargetId = std::nullopt, .targetId = target});
    table->clearInboxes();

    table->playerWithId(target).send(request::RespondPenalty{.response = core::PenaltyResponse::Challenge});

    REQUIRE(refusalReason(table->playerWithId(target).received()) == IllegalMoveReason::CannotChallenge);
}
