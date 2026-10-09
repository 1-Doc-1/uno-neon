#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
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

// The guided draw seen from the application (SPEC §4, ADR 0017): the engine says what is forced, the application
// plays it after a short pause, like the player would have.

namespace {

namespace core = uno::core;
using namespace uno::app;
using namespace std::chrono_literals;
using uno::testing::AppHarness;
using uno::testing::refusal;
using uno::testing::Table;
using uno::testing::toRequest;

// A table where `done` holds for the current round, from the first seed that gets there.
std::unique_ptr<Table> tableWhere(const std::function<bool(const core::Round&)>& done, core::DrawRule rule)
{
    for (std::uint64_t seed = 1; seed < 200; ++seed) {
        auto table = std::make_unique<Table>(3, seed, core::MatchLength::SingleRound, rule);
        table->start();
        for (std::size_t step = 0; step < 3000; ++step) {
            const auto& round = table->room().match->round();
            if (done(round)) {
                return table;
            }
            if (std::holds_alternative<core::RoundOver>(round.phase())) {
                break;
            }
            const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
            table->currentPlayer().send(toRequest(actions.at(step % actions.size())));
        }
    }
    throw std::logic_error{"no seed reached the wanted situation"};
}

bool nothingPlayable(const core::Round& round)
{
    return round.forcedAction().has_value() && std::holds_alternative<core::DrawCard>(*round.forcedAction());
}

bool drawnPlainCardToPlay(const core::Round& round)
{
    return round.forcedAction().has_value() && std::holds_alternative<core::PlayCard>(*round.forcedAction());
}

bool mustPlayInsteadOfDrawing(const core::Round& round)
{
    return std::holds_alternative<core::AwaitingPlay>(round.phase()) && !round.canDraw(round.currentPlayer());
}

} // namespace

TEST_CASE("Guided: a player with nothing to play draws by themselves after a short pause", "[app][drawRule]")
{
    auto table = tableWhere(nothingPlayable, core::DrawRule::Guided);
    const auto player = table->room().match->round().currentPlayer();
    const auto version = table->room().stateVersion;
    const auto cards = table->room().match->round().hand(player)->size();

    table->harness.scheduler.advance(1199ms);
    REQUIRE(table->room().stateVersion == version);
    table->harness.scheduler.advance(1ms);

    REQUIRE(table->room().stateVersion > version);
    const auto after = table->room().match->round().hand(player)->size();
    REQUIRE(after == cards + 1);
}

TEST_CASE("Guided: a drawn plain card that fits is played by the server after the pause", "[app][drawRule]")
{
    auto table = tableWhere(drawnPlainCardToPlay, core::DrawRule::Guided);
    const auto& round = table->room().match->round();
    const auto player = round.currentPlayer();
    const auto drawn = std::get<core::AwaitingDrawnCardDecision>(round.phase()).drawnCard;
    const auto version = table->room().stateVersion;

    table->harness.scheduler.advance(
        1200ms); // the pause before a forced move (the paced draw: application_draw_amount_test.cpp)

    REQUIRE(table->room().stateVersion > version);
    REQUIRE(table->room().match->round().discardPile().top().id == drawn);
    REQUIRE(table->room().match->round().currentPlayer() != player);
}

TEST_CASE("Guided: the server does not play for a player who can pick a card", "[app][drawRule]")
{
    auto table = tableWhere(mustPlayInsteadOfDrawing, core::DrawRule::Guided);
    const auto version = table->room().stateVersion;

    table->harness.scheduler.advance(5s);

    REQUIRE(table->room().stateVersion == version);
}

TEST_CASE("Guided: drawing while a plain card can be played is refused as MUST_PLAY", "[app][drawRule]")
{
    auto table = tableWhere(mustPlayInsteadOfDrawing, core::DrawRule::Guided);
    auto& current = table->currentPlayer();
    const auto cards = table->room().match->round().hand(current.id())->size();
    table->clearInboxes();

    current.send(request::DrawCard{});

    const auto replies = current.received();
    REQUIRE(refusal(replies) == ErrorCode::IllegalMove);
    REQUIRE(uno::testing::refusalReason(replies) == IllegalMoveReason::MustPlay);
    REQUIRE(table->room().match->round().hand(current.id())->size() == cards);
}

TEST_CASE("Guided: the view says whether the viewer can draw or keep, computed by the server", "[app][drawRule]")
{
    auto table = tableWhere(mustPlayInsteadOfDrawing, core::DrawRule::Guided);
    const auto& current = table->currentPlayer();

    const auto update = current.last<response::GameUpdate>();

    REQUIRE(update.has_value());
    REQUIRE_FALSE(update->view.game.me.canDraw);
    REQUIRE_FALSE(update->view.game.me.canKeepDrawnCard);
    REQUIRE_FALSE(update->view.game.me.playableCardIds.empty());
}

TEST_CASE("Guided: a turn that times out plays a card when drawing is not allowed", "[app][drawRule]")
{
    auto table = tableWhere(mustPlayInsteadOfDrawing, core::DrawRule::Guided);
    const auto player = table->room().match->round().currentPlayer();

    table->harness.scheduler.advance(40s); // the 30 s of the turn, after the cards the player just drew have been shown

    REQUIRE(table->room().match->round().currentPlayer() != player);
}

TEST_CASE("Official: nothing is played for the player, who may always draw", "[app][drawRule]")
{
    auto table = tableWhere(
        [](const core::Round& round) {
            return std::holds_alternative<core::AwaitingPlay>(round.phase()) && round.canDraw(round.currentPlayer());
        },
        core::DrawRule::Official);
    const auto version = table->room().stateVersion;

    table->harness.scheduler.advance(5s);

    REQUIRE(table->room().stateVersion == version);
    table->currentPlayer().send(request::DrawCard{});
    REQUIRE(table->room().stateVersion > version);
}

TEST_CASE("The draw rule is a room setting, guided by default, fixed once the match starts", "[app][drawRule]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});
    REQUIRE(host.room().settings.drawRule == core::DrawRule::Guided);

    RoomSettingsPatch patch;
    patch.drawRule = core::DrawRule::Official;
    host.send(request::UpdateSettings{.settings = patch});

    REQUIRE(host.room().settings.drawRule == core::DrawRule::Official);
}
