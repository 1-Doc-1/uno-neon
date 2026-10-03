#include "uno/app/client_message.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/testing/legal_actions.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>

// The UNO window seen from the application (SPEC §3, ADR 0018): who may catch whom, and when.

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
        if (round.unoWindow().has_value()) {
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
            const auto target = *table->room().match->round().unoWindow();
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
    auto& bystander = *std::ranges::find_if(table.players, [&](const TestPlayer& player) {
        return player.id() != target && player.id() != current;
    });
    const auto before = table.room().match->round().hand(target)->size();
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
        const auto& catchable = update->view.game.me.catchableTargetIds;
        REQUIRE(std::ranges::find(catchable, target) != catchable.end());
    }
}

// G1 (bug): the catch must stay possible for the players who are not next, after the next player has acted.
TEST_CASE("A bystander can still catch after the player whose turn it is has acted", "[app][uno][window]")
{
    auto window = tableWithOpenWindow();
    Table& table = *window.table;
    const auto target = window.target;
    const auto current = table.room().match->round().currentPlayer();
    auto& bystander = *std::ranges::find_if(table.players, [&](const TestPlayer& player) {
        return player.id() != target && player.id() != current;
    });
    // The player on turn draws a card: before ADR 0018 this closed the window.
    table.playerWithId(current).send(request::DrawCard{});
    REQUIRE(table.room().match->round().unoWindow() == target);
    table.clearInboxes();

    bystander.send(request::CatchUno{.targetId = target});

    REQUIRE_FALSE(refusal(bystander.received()).has_value());
}
