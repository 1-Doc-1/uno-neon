#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
#include "uno/core/playability.hpp"
#include "uno/core/player_action.hpp"
#include "uno/testing/legal_actions.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>

// The house rule "declare UNO to win" seen from the application (SPEC §4, ADR 0019).

namespace {

namespace core = uno::core;
using namespace uno::app;
using namespace std::chrono_literals;
using uno::testing::AppHarness;
using uno::testing::refusal;
using uno::testing::Table;
using uno::testing::toRequest;

// A table, playing the rule, where the current player is stuck on their last card for want of an announcement.
// With `plainCard`, that card is not a special one (a player holding a special one may draw instead).
std::unique_ptr<Table> tableWhereBlocked(core::DrawRule drawRule, bool plainCard = false)
{
    for (std::uint64_t seed = 1; seed < 400; ++seed) {
        auto table = std::make_unique<Table>(3, seed, core::MatchLength::SingleRound, drawRule, true);
        table->start();
        for (std::size_t step = 0; step < 3000; ++step) {
            const auto& round = table->room().match->round();
            if (round.mustDeclareUno(round.currentPlayer()) &&
                (!plainCard || !core::isSpecialCard(round.hand(round.currentPlayer())->front()))) {
                return table;
            }
            if (std::holds_alternative<core::RoundOver>(round.phase())) {
                break;
            }
            const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
            table->currentPlayer().send(toRequest(actions.at(step % actions.size())));
        }
    }
    throw std::logic_error{"no seed reached a player stuck on their last card"};
}

} // namespace

TEST_CASE("The rule is a room setting, off by default", "[app][declareUno]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});
    REQUIRE_FALSE(host.room().settings.declareUnoToWin);

    RoomSettingsPatch patch;
    patch.declareUnoToWin = true;
    host.send(request::UpdateSettings{.settings = patch});

    REQUIRE(host.room().settings.declareUnoToWin);
}

TEST_CASE("The last card is refused as MUST_DECLARE_UNO, and the view says so", "[app][declareUno]")
{
    auto table = tableWhereBlocked(core::DrawRule::Official);
    auto& current = table->currentPlayer();
    const auto hand =
        *table->room().match->round().hand(current.id()); // a span: copied, not a reference to a temporary
    REQUIRE(hand.size() == 1);
    REQUIRE(current.last<response::GameUpdate>()->view.game.me.mustDeclareUno);
    table->clearInboxes();

    current.send(uno::testing::playRequest(hand.front(), table->room().match->round()));

    const auto replies = current.received();
    REQUIRE(refusal(replies) == ErrorCode::IllegalMove);
    REQUIRE(uno::testing::refusalReason(replies) == IllegalMoveReason::MustDeclareUno);
    REQUIRE(table->room().match->round().hand(current.id())->size() == 1);
}

TEST_CASE("Announcing UNO unblocks the last card", "[app][declareUno]")
{
    auto table = tableWhereBlocked(core::DrawRule::Official);
    auto& current = table->currentPlayer();
    const auto card = table->room().match->round().hand(current.id())->front();

    current.send(request::CallUno{});
    REQUIRE_FALSE(current.last<response::GameUpdate>()->view.game.me.mustDeclareUno);
    current.send(uno::testing::playRequest(card, table->room().match->round()));

    REQUIRE(std::holds_alternative<core::RoundOver>(table->room().match->round().phase()));
}

TEST_CASE("Guided draw: a card blocked by the missing UNO never makes the server draw", "[app][declareUno][drawRule]")
{
    auto table = tableWhereBlocked(core::DrawRule::Guided, true);
    const auto player = table->room().match->round().currentPlayer();
    const auto version = table->room().stateVersion;

    table->harness.scheduler.advance(1500ms); // twice the pause of a forced move

    REQUIRE(table->room().stateVersion == version);
    REQUIRE(table->room().match->round().hand(player)->size() == 1);
    REQUIRE(table->room().match->round().currentPlayer() == player);
}

TEST_CASE("Guided draw: a turn that times out while blocked announces UNO and plays the card", "[app][declareUno]")
{
    auto table = tableWhereBlocked(core::DrawRule::Guided, true);
    REQUIRE_FALSE(table->room().match->round().canDraw(table->room().match->round().currentPlayer()));

    table->harness.scheduler.advance(31s);

    REQUIRE(std::holds_alternative<core::RoundOver>(table->room().match->round().phase()));
}
