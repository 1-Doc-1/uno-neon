#include "uno/app/client_message.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
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
#include <stdexcept>
#include <variant>

// The UNO window seen from the application (SPEC §3, ADR 0018): who may catch whom, and when.

using namespace std::chrono_literals;

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::ofType;
using uno::testing::refusal;
using uno::testing::Table;
using uno::testing::TestPlayer;
using uno::testing::toRequest;

// Plays legal actions until a player has just left themselves with one unannounced card. Returns false if the round
// ended first.
bool playUntilUnoWindow(Table& table)
{
    for (std::size_t step = 0; step < 4000; ++step) {
        const auto& round = table.room().match->round();
        if (!round.unoWindows().empty()) {
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

// A table of three where a window is open, from the first seed that produces one.
// The table is heap-allocated: its players point back at its harness, so it must not move.
struct OpenWindow {
    std::unique_ptr<Table> table;
    core::PlayerId target;
};

OpenWindow tableWithOpenWindow()
{
    for (std::uint64_t seed = 1; seed < 200; ++seed) {
        auto table = std::make_unique<Table>(3, seed);
        table->start();
        if (playUntilUnoWindow(*table)) {
            const auto target = table->room().match->round().unoWindows().front();
            return {.table = std::move(table), .target = target};
        }
    }
    throw std::logic_error{"no seed produced a UNO window"};
}

} // namespace

TEST_CASE("A player whose turn it is not can catch the player left with one unannounced card", "[app][uno]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;
    const auto current = table.room().match->round().currentPlayer();
    auto& bystander = *std::ranges::find_if(
        table.players, [&](const TestPlayer& player) { return player.id() != target && player.id() != current; });
    const auto before = table.room().match->round().hand(target)->size();
    table.harness.scheduler.advance(2s); // the grace period: only the offender could announce until now
    table.clearInboxes();

    bystander.send(request::CatchUno{.targetId = target});

    const auto replies = bystander.received();
    REQUIRE_FALSE(refusal(replies).has_value());
    REQUIRE(table.room().match->round().hand(target)->size() == before + core::kUnoPenaltyCards);
    const auto update = ofType<response::GameUpdate>(table.playerWithId(target).received());
    REQUIRE_FALSE(update.empty());
}

TEST_CASE("Every other player is offered the catch in their view, whoever's turn it is", "[app][uno]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;

    for (auto& player : table.players) {
        const auto update = player.last<response::GameUpdate>();
        if (!update || player.id() == target) {
            continue;
        }
        const auto& windows = update->view.unoWindows;
        REQUIRE(windows.size() == 1);
        REQUIRE(windows.front().targetId == target);
        REQUIRE(windows.front().graceEndsAt < windows.front().expiresAt);
    }
}

// G1 (bug): the catch must stay possible for the players who are not next, after the next player has acted.
TEST_CASE("A bystander can still catch after the player whose turn it is has acted", "[app][uno][window]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;
    const auto current = table.room().match->round().currentPlayer();
    auto& bystander = *std::ranges::find_if(
        table.players, [&](const TestPlayer& player) { return player.id() != target && player.id() != current; });
    // The player on turn draws a card: before ADR 0018 this closed the window.
    table.playerWithId(current).send(request::DrawCard{});
    REQUIRE(table.room().match->round().hasUnoWindowOn(target));
    table.harness.scheduler.advance(2s);
    table.clearInboxes();

    bystander.send(request::CatchUno{.targetId = target});

    REQUIRE_FALSE(refusal(bystander.received()).has_value());
}

TEST_CASE("During the grace period only the offender may act: others are told it is too early", "[app][uno][window]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;
    auto& bystander =
        *std::ranges::find_if(table.players, [&](const TestPlayer& player) { return player.id() != target; });
    const auto before = table.room().match->round().hand(target)->size();
    table.harness.scheduler.advance(1999ms);
    table.clearInboxes();

    bystander.send(request::CatchUno{.targetId = target});

    REQUIRE(refusal(bystander.received()) == ErrorCode::UnoGracePeriod);
    REQUIRE(table.room().match->round().hand(target)->size() == before);
    REQUIRE(table.room().match->round().hasUnoWindowOn(target));
}

TEST_CASE("An offender who announces during the grace period cannot be caught afterwards", "[app][uno][window]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;
    auto& offender = table.playerWithId(target);
    auto& bystander =
        *std::ranges::find_if(table.players, [&](const TestPlayer& player) { return player.id() != target; });
    table.harness.scheduler.advance(1s);

    offender.send(request::CallUno{});
    REQUIRE_FALSE(refusal(offender.received()).has_value());
    table.harness.scheduler.advance(5s);
    table.clearInboxes();
    bystander.send(request::CatchUno{.targetId = target});

    REQUIRE(refusal(bystander.received()) == ErrorCode::UnoWindowClosed);
    REQUIRE(table.room().match->round().unoWindows().empty());
    const auto update = bystander.last<response::GameUpdate>();
    REQUIRE(update.has_value());
    REQUIRE(update->view.unoWindows.empty());
}

TEST_CASE("The window runs out fifteen seconds after it opened, and everybody is told", "[app][uno][window]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;
    auto& bystander =
        *std::ranges::find_if(table.players, [&](const TestPlayer& player) { return player.id() != target; });
    table.harness.scheduler.advance(14999ms);
    REQUIRE(table.room().match->round().hasUnoWindowOn(target));
    table.clearInboxes();

    table.harness.scheduler.advance(1ms);

    REQUIRE(table.room().match->round().unoWindows().empty());
    const auto update = bystander.last<response::GameUpdate>();
    REQUIRE(update.has_value());
    REQUIRE(update->view.unoWindows.empty());
    bystander.send(request::CatchUno{.targetId = target});
    REQUIRE(refusal(bystander.received()) == ErrorCode::UnoWindowClosed);
}

TEST_CASE("The first catch wins and the second one is told the window is closed", "[app][uno][window]")
{
    for (std::uint64_t seed = 1; seed < 200; ++seed) {
        Table table(4, seed);
        table.start();
        if (!playUntilUnoWindow(table)) {
            continue;
        }
        const auto target = table.room().match->round().unoWindows().front();
        std::vector<TestPlayer*> catchers;
        for (auto& player : table.players) {
            if (player.id() != target) {
                catchers.push_back(&player);
            }
        }
        table.harness.scheduler.advance(2s);
        const auto before = table.room().match->round().hand(target)->size();

        catchers.at(0)->send(request::CatchUno{.targetId = target});
        catchers.at(1)->send(request::CatchUno{.targetId = target});

        REQUIRE(table.room().match->round().hand(target)->size() == before + core::kUnoPenaltyCards);
        REQUIRE(refusal(catchers.at(1)->received()) == ErrorCode::UnoWindowClosed);
        return;
    }
    FAIL("no seed produced a UNO window");
}

// ADR 0020: a broadcast that does not change the turn (here, a UNO window running out) keeps the turn clock running.
TEST_CASE("A window running out does not give the player on turn a fresh clock", "[app][uno][window][turn]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto deadline = table.room().turnDeadline;
    const auto current = table.room().match->round().currentPlayer();
    REQUIRE(deadline.has_value());

    table.harness.scheduler.advance(15s);

    REQUIRE(table.room().match->round().unoWindows().empty());
    REQUIRE(table.room().turnDeadline == deadline);
    REQUIRE(table.room().match->round().currentPlayer() == current);
}
