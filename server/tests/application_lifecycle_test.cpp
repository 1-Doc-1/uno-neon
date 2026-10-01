#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/legal_actions.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include "support/require.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

// Everything that happens because time passes or people leave (step 2.5): grace periods, inactivity, session
// expiry, the turn timer and the next-round deadline. Time is the ManualScheduler's: no test waits.

namespace {

namespace core = uno::core;
using namespace std::chrono_literals;
using namespace uno::app;
using uno::testing::AppHarness;
using uno::testing::ofType;
using uno::testing::refusal;
using uno::testing::require;
using uno::testing::Table;
using uno::testing::toRequest;

// Plays legal actions, one per step, until `done` holds or the budget runs out. Returns whether it holds.
bool playUntil(Table& table, const std::function<bool(const core::Round&)>& done, std::size_t budget = 3000)
{
    for (std::size_t step = 0; step < budget; ++step) {
        const auto& round = table.room().match->round();
        if (done(round)) {
            return true;
        }
        if (std::holds_alternative<core::RoundOver>(round.phase())) {
            return false;
        }
        const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
        table.currentPlayer().send(toRequest(actions.at(step % actions.size())));
    }
    return false;
}

std::optional<response::GameUpdate> lastUpdate(const uno::testing::TestPlayer& player)
{
    return player.last<response::GameUpdate>();
}

} // namespace

TEST_CASE("An idle lobby closes after 15 minutes, and any activity restarts the count", "[app][lifecycle]")
{
    Table table(2);
    table.clearInboxes();

    table.harness.scheduler.advance(14min);
    table.players.back().send(request::SetReady{.ready = false});
    table.harness.scheduler.advance(14min);
    REQUIRE(table.harness.rooms.size() == 1);
    table.clearInboxes();
    table.harness.scheduler.advance(2min);

    REQUIRE(table.harness.rooms.size() == 0);
    for (auto& player : table.players) {
        const auto closed = ofType<response::RoomClosed>(player.received());
        REQUIRE(closed.size() == 1);
        REQUIRE(closed.front().reason == response::RoomClosedReason::Expired);
    }
    table.players.back().send(request::JoinRoom{.code = table.code, .nickname = "Late"});
    REQUIRE(refusal(table.players.back().received()) == ErrorCode::RoomNotFound);
}

TEST_CASE("A finished match closes its room after 5 minutes", "[app][lifecycle]")
{
    Table table(2);
    table.start();
    REQUIRE(playUntil(table, [&table](const core::Round&) { return table.room().match->winner().has_value(); }));
    REQUIRE(table.room().phase == response::RoomPhase::MatchOver);

    table.harness.scheduler.advance(4min);
    REQUIRE(table.harness.rooms.size() == 1);
    table.harness.scheduler.advance(2min);

    REQUIRE(table.harness.rooms.size() == 0);
}

TEST_CASE("A disconnected lobby player is removed after the grace period unless they return", "[app][lifecycle]")
{
    Table table(3);
    const auto& host = table.players.at(0);
    const auto leaver = table.players.at(1);
    const auto stayer = table.players.at(2);
    table.harness.application.onDisconnected(leaver.connection());
    table.harness.application.onDisconnected(stayer.connection());

    table.harness.scheduler.advance(59s);
    auto back = table.harness.connect();
    back.hello(stayer.token());
    table.harness.scheduler.advance(5s);

    const auto room = host.room();
    REQUIRE(room.players.size() == 2);
    REQUIRE(
        std::ranges::none_of(room.players, [&leaver](const auto& member) { return member.playerId == leaver.id(); }));
    REQUIRE(
        std::ranges::any_of(room.players, [&stayer](const auto& member) { return member.playerId == stayer.id(); }));
}

TEST_CASE("The host who does not come back is replaced by the next connected player", "[app][lifecycle]")
{
    Table table(3);
    table.harness.application.onDisconnected(table.players.at(0).connection());

    table.harness.scheduler.advance(61s);

    const auto room = table.players.at(1).room();
    REQUIRE(room.players.size() == 2);
    const auto newHost = *std::ranges::find_if(room.players, [](const auto& member) { return member.isHost; });
    REQUIRE(newHost.playerId == table.players.at(1).id());
}

TEST_CASE("A player who never comes back during a match is taken out of it, and the match goes on", "[app][lifecycle]")
{
    Table table(3);
    table.start();
    const auto absent = table.players.at(2);
    table.harness.application.onDisconnected(absent.connection());
    table.clearInboxes();

    table.harness.scheduler.advance(61s);

    const auto update = require(lastUpdate(table.players.at(0)));
    REQUIRE(update.view.game.players.size() == 2);
    REQUIRE(update.view.seats.size() == 2);
    REQUIRE(table.room().match->round().seats().size() == 2);
    REQUIRE(table.room().phase == response::RoomPhase::InGame);
    auto& current = table.currentPlayer();
    static_cast<void>(current.received());
    current.send(toRequest(uno::testing::legalActionsOfCurrentPlayer(table.room().match->round()).front()));
    REQUIRE_FALSE(refusal(current.received()).has_value());
}

TEST_CASE("With two players, the one who stays wins by forfeit when the other never comes back", "[app][lifecycle]")
{
    Table table(2);
    table.start();
    table.harness.application.onDisconnected(table.players.at(1).connection());
    table.clearInboxes();

    table.harness.scheduler.advance(61s);

    REQUIRE(table.room().phase == response::RoomPhase::MatchOver);
    const auto update = require(lastUpdate(table.players.at(0)));
    REQUIRE(update.view.game.phase == core::ViewPhase::MatchOver);
    REQUIRE(update.view.game.matchWinnerId == table.players.at(0).id());
    REQUIRE(std::ranges::any_of(update.events, [](const core::ClientEvent& event) {
        return std::holds_alternative<core::MatchEndedEvent>(event);
    }));
    // The seat of the player who left is still named in the final view.
    REQUIRE(update.view.seats.size() == 2);
    REQUIRE(update.view.seats.at(1).nickname == "Player1");
}

TEST_CASE("Leaving a running match takes the player out of it, and a leaving host hands over", "[app][lifecycle]")
{
    Table table(3);
    table.start();
    table.clearInboxes();

    table.players.at(0).send(request::LeaveRoom{});

    const auto update = require(lastUpdate(table.players.at(1)));
    REQUIRE(update.view.game.players.size() == 2);
    const auto hostChange = std::ranges::find_if(update.events, [](const core::ClientEvent& event) {
        return std::holds_alternative<core::HostChangedEvent>(event);
    });
    REQUIRE(hostChange != update.events.end());
    REQUIRE(std::get<core::HostChangedEvent>(*hostChange).playerId == table.players.at(1).id());
    REQUIRE(table.room().host == table.players.at(1).id());
}

TEST_CASE("A player removed by the grace period resumes their session outside the room", "[app][lifecycle]")
{
    Table table(3);
    const auto absent = table.players.at(2);
    table.harness.application.onDisconnected(absent.connection());
    table.harness.scheduler.advance(61s);

    auto back = table.harness.connect();
    back.hello(absent.token());

    const auto welcome = require(back.last<response::Welcome>());
    REQUIRE(welcome.playerId == absent.id());
    REQUIRE_FALSE(welcome.resumedRoomCode.has_value());
}

TEST_CASE("A session nobody uses is forgotten after 10 minutes", "[app][lifecycle]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();
    const auto& token = player.token();
    harness.application.onDisconnected(player.connection());

    harness.scheduler.advance(9min);
    auto early = harness.connect();
    early.hello(token);
    REQUIRE(early.last<response::Welcome>().has_value());
    harness.application.onDisconnected(early.connection());
    harness.scheduler.advance(11min);

    auto late = harness.connect();
    late.hello(token);
    REQUIRE(refusal(late.received()) == ErrorCode::SessionExpired);
}

TEST_CASE("The turn deadline is shown, and a turn that times out draws a card and passes", "[app][lifecycle][turn]")
{
    Table table(3);
    table.start();
    auto& current = table.currentPlayer();
    const auto first = require(lastUpdate(current));
    REQUIRE(first.view.turnDeadline == table.harness.clock.nowMillis() + 30'000);
    const auto before = table.room().match->round().currentPlayer();
    table.clearInboxes();

    table.harness.scheduler.advance(29s);
    REQUIRE(table.room().match->round().currentPlayer() == before);
    table.harness.scheduler.advance(2s);

    REQUIRE(table.room().match->round().currentPlayer() != before);
    const auto update = require(lastUpdate(table.players.at(0)));
    REQUIRE(std::ranges::any_of(update.events, [&before](const core::ClientEvent& event) {
        const auto* drawn = std::get_if<core::CardsDrawnEvent>(&event);
        return drawn != nullptr && drawn->playerId == before;
    }));
    REQUIRE(update.view.turnDeadline.has_value());
}

TEST_CASE("Without a turn timer, nothing is armed and no deadline is shown", "[app][lifecycle][turn]")
{
    Table table(2);
    table.players.front().send(request::UpdateSettings{
        .settings =
            [] {
                RoomSettingsPatch patch;
                patch.turnTimer = TurnTimerSeconds::Off;
                return patch;
            }(),
    });
    table.start();

    const auto update = require(lastUpdate(table.players.front()));
    REQUIRE_FALSE(update.view.turnDeadline.has_value());
    const auto before = table.room().match->round().currentPlayer();
    table.harness.scheduler.advance(10min);
    REQUIRE(table.room().match->round().currentPlayer() == before);
}

TEST_CASE("A move made in time disarms the old timer", "[app][lifecycle][turn]")
{
    Table table(3);
    table.start();
    table.harness.scheduler.advance(20s);
    const auto actions = uno::testing::legalActionsOfCurrentPlayer(table.room().match->round());
    table.currentPlayer().send(toRequest(actions.front()));
    const auto after = table.room().match->round().currentPlayer();
    const auto version = table.room().stateVersion;

    table.harness.scheduler.advance(15s); // the first timer would have fired by now

    REQUIRE(table.room().stateVersion == version);
    REQUIRE(table.room().match->round().currentPlayer() == after);
}

TEST_CASE("A timed-out turn closes the UNO window like any draw would", "[app][lifecycle][turn]")
{
    bool seenOpenWindow = false;
    for (std::uint64_t seed = 1; seed <= 12 && !seenOpenWindow; ++seed) {
        Table table(3, seed);
        table.start();
        if (!playUntil(table, [](const core::Round& round) { return round.unoWindow().has_value(); })) {
            continue;
        }
        seenOpenWindow = true;

        table.harness.scheduler.advance(31s);

        REQUIRE_FALSE(table.room().match->round().unoWindow().has_value());
    }
    REQUIRE(seenOpenWindow);
}

TEST_CASE("A pending penalty is accepted when the turn times out", "[app][lifecycle][turn]")
{
    bool seen = false;
    for (std::uint64_t seed = 1; seed <= 40 && !seen; ++seed) {
        Table table(3, seed);
        table.start();
        const auto penaltyPending = [](const core::Round& round) {
            return std::holds_alternative<core::AwaitingPenaltyResponse>(round.phase());
        };
        if (!playUntil(table, penaltyPending)) {
            continue;
        }
        seen = true;

        table.harness.scheduler.advance(31s);

        REQUIRE_FALSE(penaltyPending(table.room().match->round()));
    }
    REQUIRE(seen);
}

TEST_CASE("A card drawn at timeout is not played: the turn passes", "[app][lifecycle][turn]")
{
    bool seen = false;
    for (std::uint64_t seed = 1; seed <= 40 && !seen; ++seed) {
        Table table(3, seed);
        table.start();
        const auto deciding = [](const core::Round& round) {
            return std::holds_alternative<core::AwaitingDrawnCardDecision>(round.phase());
        };
        if (!playUntil(table, deciding)) {
            continue;
        }
        seen = true;
        const auto decider = table.room().match->round().currentPlayer();

        table.harness.scheduler.advance(31s);

        REQUIRE(table.room().match->round().currentPlayer() != decider);
    }
    REQUIRE(seen);
}

TEST_CASE("The next round starts by itself 30 seconds after a round ends", "[app][lifecycle][round]")
{
    Table table(3, 5, core::MatchLength::To500);
    table.start();
    static_cast<void>(playUntil(table, [](const core::Round&) { return false; })); // plays the round out
    REQUIRE(std::holds_alternative<core::RoundOver>(table.room().match->round().phase()));
    const auto over = require(lastUpdate(table.players.at(0)));
    REQUIRE(over.view.nextRoundDeadline == table.harness.clock.nowMillis() + 30'000);
    table.clearInboxes();

    table.harness.scheduler.advance(29s);
    REQUIRE(table.room().match->roundNumber() == 1);
    table.harness.scheduler.advance(2s);

    REQUIRE(table.room().match->roundNumber() == 2);
    REQUIRE(require(lastUpdate(table.players.at(0))).view.game.round == 2);
}
