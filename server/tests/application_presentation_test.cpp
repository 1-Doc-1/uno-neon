#include "uno/app/client_message.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/playability.hpp"
#include "uno/testing/legal_actions.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <variant>

// Nobody acts while the effect in progress is being shown (ADR 0027): the application budgets what an action costs to
// present and refuses the turn actions that come before.

using namespace std::chrono_literals;

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::refusal;
using uno::testing::Table;
using uno::testing::toRequest;

// A Draw Two the player on turn can play, if they hold one.
std::optional<core::Card> playableDrawTwo(const core::Round& round)
{
    if (!std::holds_alternative<core::AwaitingPlay>(round.phase())) {
        return std::nullopt;
    }
    const auto hand = round.hand(round.currentPlayer()).value_or(std::span<const core::Card>{});
    const auto found = std::ranges::find_if(hand, [&round](const core::Card& card) {
        return card.rank == core::Rank::DrawTwo &&
               core::isPlayable(card, round.discardPile().top(), round.currentColor());
    });
    return found == hand.end() ? std::nullopt : std::optional<core::Card>(*found);
}

// `players` players, the real pace of the game, and the player on turn about to play a Draw Two. While searching, the
// clock jumps over every pause so that the scripted players are never refused for acting too early.
std::unique_ptr<Table> tableAboutToPlayDrawTwo(std::size_t players)
{
    for (std::uint64_t seed = 1; seed < 300; ++seed) {
        auto table = std::make_unique<Table>(players, seed, core::MatchLength::SingleRound, core::DrawRule::Guided,
                                             false, core::DrawAmount::One, Timeouts{});
        table->start();
        for (std::size_t step = 0; step < 3000; ++step) {
            const auto& round = table->room().match->round();
            if (playableDrawTwo(round)) {
                table->harness.clock.advanceMillis(60'000);
                return table;
            }
            if (std::holds_alternative<core::RoundOver>(round.phase())) {
                break;
            }
            const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
            table->harness.clock.advanceMillis(60'000);
            table->currentPlayer().send(toRequest(actions.at(step % actions.size())));
        }
    }
    throw std::logic_error{"no seed reached a playable Draw Two"};
}

// The first legal action that is a turn action (not an announcement or a catch).
core::PlayerAction firstTurnAction(const core::Round& round)
{
    const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
    const auto found = std::ranges::find_if(actions, [](const core::PlayerAction& action) {
        return !std::holds_alternative<core::CallUno>(action) && !std::holds_alternative<core::CatchUno>(action);
    });
    return *found;
}

struct DrawTwoPlayed {
    std::unique_ptr<Table> table;
    core::PlayerId attacker;
    core::PlayerId next; // the player who gets the turn after the victim is skipped
    std::int64_t playedAt{};
};

DrawTwoPlayed drawTwoPlayed(std::size_t players = 3)
{
    auto table = tableAboutToPlayDrawTwo(players);
    const auto attacker = table->room().match->round().currentPlayer();
    const auto card = playableDrawTwo(table->room().match->round());
    const auto playedAt = table->harness.clock.nowMillis();
    table->clearInboxes();
    table->currentPlayer().send(request::PlayCard{
        .cardId = card->id,
        .chosenColor = std::nullopt,
        .swapTargetId = std::nullopt,
    });
    const auto next = table->room().match->round().currentPlayer();
    return {.table = std::move(table), .attacker = attacker, .next = next, .playedAt = playedAt};
}

// What presenting a Draw Two takes: the card, the "+2", the skipped victim and the two cards they draw.
constexpr auto kDrawTwoBudget = 1100ms + 1200ms + 1200ms + 2 * 1000ms;

} // namespace

TEST_CASE("The view says when turn actions open, after the card, the effects and the cards drawn",
          "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed();

    const auto view = table->playerWithId(next).last<response::GameUpdate>()->view;

    REQUIRE(view.actionsOpenAt == playedAt + kDrawTwoBudget.count());
    REQUIRE(table->room().actionsOpenAt == view.actionsOpenAt);
}

TEST_CASE("A turn action received before actionsOpenAt is refused, then accepted once the effect is over",
          "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed();
    auto& player = table->playerWithId(next);
    const auto action = toRequest(firstTurnAction(table->room().match->round()));
    const auto version = table->room().stateVersion;

    table->harness.scheduler.advance(kDrawTwoBudget - 1ms);
    table->clearInboxes();
    player.send(action);

    REQUIRE(refusal(player.received()) == ErrorCode::EffectInProgress);
    REQUIRE(table->room().stateVersion == version);

    table->harness.scheduler.advance(1ms);
    table->clearInboxes();
    player.send(action);

    REQUIRE_FALSE(refusal(player.received()).has_value());
    REQUIRE(table->room().stateVersion > version);
}

TEST_CASE("Announcing UNO and catching are not held back by the effect in progress", "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed();
    auto& player = table->playerWithId(next);

    table->clearInboxes();
    player.send(request::CallUno{});
    const auto announced = refusal(player.received());
    player.send(request::CatchUno{.targetId = attacker});
    const auto caught = refusal(player.received());

    // Refused by the rules of UNO (nothing to announce, nobody to catch), never because of the effect
    REQUIRE(announced != ErrorCode::EffectInProgress);
    REQUIRE(caught != ErrorCode::EffectInProgress);
}

TEST_CASE("A player who is not on turn is told so, whatever the time", "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed();
    auto& bystander = table->playerWithId(attacker);

    table->clearInboxes();
    bystander.send(request::DrawCard{});

    REQUIRE(refusal(bystander.received()) == ErrorCode::NotYourTurn);
}

TEST_CASE("The clock of the turn starts when the effect in progress is over", "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed();
    const auto& room = table->room();

    const auto seconds = std::chrono::seconds(static_cast<int>(room.settings.turnTimer));
    REQUIRE(room.turnDeadline.value() == room.actionsOpenAt + std::chrono::milliseconds(seconds).count());
    REQUIRE(room.actionsOpenAt == playedAt + kDrawTwoBudget.count());
}

TEST_CASE(
    "With two players the one who played the Draw Two gets a new clock, which also starts when the effect is over",
    "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed(2);
    const auto& room = table->room();

    REQUIRE(next == attacker); // the victim is skipped: same player on turn, but a new turn
    const auto seconds = std::chrono::seconds(static_cast<int>(room.settings.turnTimer));
    REQUIRE(room.turnDeadline.value() == playedAt + (kDrawTwoBudget + seconds).count());
}

TEST_CASE("A forced move waits for the effect in progress, then for its own pause", "[app][presentation]")
{
    auto [table, attacker, next, playedAt] = drawTwoPlayed();
    const auto version = table->room().stateVersion;
    if (!table->room().match->round().forcedAction().has_value()) {
        SUCCEED("the player who gets the turn has a card to choose: no forced move here");
        return;
    }

    table->harness.scheduler.advance(kDrawTwoBudget + 1199ms);
    REQUIRE(table->room().stateVersion == version);
    table->harness.scheduler.advance(1ms);
    REQUIRE(table->room().stateVersion == version + 1);
}

TEST_CASE("Nothing is held back when the pace is zero", "[app][presentation]")
{
    Table table(2, 7, core::MatchLength::SingleRound);
    table.start();
    auto& actor = table.currentPlayer();
    const auto action = toRequest(firstTurnAction(table.room().match->round()));

    table.clearInboxes();
    actor.send(action);

    REQUIRE(table.room().actionsOpenAt == table.harness.clock.nowMillis());
    REQUIRE_FALSE(refusal(actor.received()).has_value());
}
