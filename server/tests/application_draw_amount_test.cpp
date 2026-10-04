#include "uno/app/client_message.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/draw_amount.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
#include "uno/core/playability.hpp"
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
#include <utility>
#include <variant>

// The draw amount seen from the application (SPEC §4, ADR 0024): the whole draw is one action, one broadcast, and the
// opponents only learn how many cards were drawn.

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::AppHarness;
using uno::testing::Table;
using uno::testing::toRequest;

// A table where the current player has nothing to play and the top two cards of the draw pile do not fit either, so
// that drawing takes at least three cards.
std::unique_ptr<Table> longDrawAtSeed(std::uint64_t seed, core::DrawRule rule)
{
    const auto longDraw = [](const core::Round& round) {
        if (!std::holds_alternative<core::AwaitingPlay>(round.phase()) || round.drawPile().cards().size() < 3) {
            return false;
        }
        const auto playable = [&round](const core::Card& card) {
            return core::isPlayable(card, round.discardPile().top(), round.currentColor());
        };
        const auto hand = round.hand(round.currentPlayer()).value_or(std::span<const core::Card>{});
        const auto cards = round.drawPile().cards();
        return std::ranges::none_of(hand, playable) && !playable(cards.back()) && !playable(*(cards.end() - 2));
    };
    auto table =
        std::make_unique<Table>(3, seed, core::MatchLength::SingleRound, rule, false, core::DrawAmount::UntilPlayable);
    table->start();
    for (std::size_t step = 0; step < 3000; ++step) {
        const auto& round = table->room().match->round();
        if (longDraw(round)) {
            return table;
        }
        if (std::holds_alternative<core::RoundOver>(round.phase())) {
            break;
        }
        const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
        table->currentPlayer().send(toRequest(actions.at(step % actions.size())));
    }
    return nullptr;
}

std::unique_ptr<Table> tableWithALongDraw(core::DrawRule rule)
{
    for (std::uint64_t seed = 1; seed < 300; ++seed) {
        if (auto table = longDrawAtSeed(seed, rule)) {
            return table;
        }
    }
    throw std::logic_error{"no seed reached a long draw"};
}

[[nodiscard]] std::size_t cardsAnnounced(const response::GameUpdate& update, const core::PlayerId& drawer)
{
    std::size_t total = 0;
    for (const auto& event : update.events) {
        if (const auto* drawn = std::get_if<core::CardsDrawnEvent>(&event);
            drawn != nullptr && drawn->playerId == drawer) {
            total += drawn->count;
        }
    }
    return total;
}

} // namespace

TEST_CASE("Rooms draw until a card fits by default", "[app][drawAmount]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});

    REQUIRE(host.room().settings.drawAmount == core::DrawAmount::UntilPlayable);
}

TEST_CASE("A long draw is one action, one broadcast, one event per card", "[app][drawAmount]")
{
    auto table = tableWithALongDraw(core::DrawRule::Official);
    auto& drawer = table->currentPlayer();
    const auto drawerId = drawer.id();
    const auto cards = table->room().match->round().hand(drawerId)->size();
    const auto version = table->room().stateVersion;
    table->clearInboxes();

    drawer.send(request::DrawCard{});

    REQUIRE(table->room().stateVersion == version + 1);
    const auto grown = table->room().match->round().hand(drawerId)->size() - cards;
    REQUIRE(grown >= 3);
    const auto update = drawer.last<response::GameUpdate>();
    REQUIRE(update.has_value());
    const auto drawnEvents = std::ranges::count_if(update->events, [](const core::ClientEvent& event) {
        return std::holds_alternative<core::CardsDrawnEvent>(event);
    });
    REQUIRE(std::cmp_equal(drawnEvents, grown));
}

TEST_CASE("Opponents see how many cards were drawn, never which", "[app][drawAmount]")
{
    auto table = tableWithALongDraw(core::DrawRule::Official);
    const auto drawerId = table->currentPlayer().id();
    table->clearInboxes();

    table->currentPlayer().send(request::DrawCard{});

    for (auto& other : table->players) {
        const auto update = other.last<response::GameUpdate>();
        REQUIRE(update.has_value());
        for (const auto& event : update->events) {
            const auto* drawn = std::get_if<core::CardsDrawnEvent>(&event);
            if (drawn == nullptr || drawn->playerId != drawerId) {
                continue;
            }
            REQUIRE(drawn->cards.has_value() == (other.id() == drawerId));
        }
        REQUIRE(cardsAnnounced(*update, drawerId) >= 3);
    }
}

namespace {

// A table where the forced draw of the current player takes at least three cards and leaves a card the server must
// play by itself.
struct PacedDraw {
    std::unique_ptr<Table> table;
    core::PlayerId drawer;
};

PacedDraw tableWithAForcedLongDrawThenPlay()
{
    for (std::uint64_t seed = 1; seed < 300; ++seed) {
        auto table = longDrawAtSeed(seed, core::DrawRule::Guided);
        if (table == nullptr) {
            continue;
        }
        const auto drawer = table->room().match->round().currentPlayer();
        table->harness.scheduler.advance(std::chrono::milliseconds(1200)); // the forced draw
        const auto forced = table->room().match->round().forcedAction();
        if (forced.has_value() && std::holds_alternative<core::PlayCard>(*forced)) {
            return {.table = std::move(table), .drawer = drawer};
        }
    }
    throw std::logic_error{"no seed gave a long draw followed by a forced play"};
}

} // namespace

TEST_CASE("The forced play waits for the end of the paced draw, and so does the clock of the turn",
          "[app][drawAmount][pace]")
{
    using namespace std::chrono_literals;
    auto [table, drawer] = tableWithAForcedLongDrawThenPlay();
    const auto& room = table->room();
    const auto version = room.stateVersion;
    const auto drawn =
        static_cast<std::int64_t>(cardsAnnounced(*table->players.front().last<response::GameUpdate>(), drawer));
    REQUIRE(drawn >= 3);
    const auto pause = std::chrono::milliseconds(drawn * 1000);
    const auto turnClock = room.turnDeadline.value() - table->harness.clock.nowMillis();
    REQUIRE(turnClock == (std::chrono::seconds(static_cast<int>(room.settings.turnTimer)) + pause).count());

    table->harness.scheduler.advance(pause + 1199ms);
    REQUIRE(room.stateVersion == version);
    table->harness.scheduler.advance(1ms);
    REQUIRE(room.stateVersion == version + 1);
}
