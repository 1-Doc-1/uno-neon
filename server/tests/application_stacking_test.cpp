#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/card.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
#include "uno/core/penalty_stacking.hpp"
#include "uno/core/playability.hpp"
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

// Penalty stacking through the application (ADR 0029): the host's setting reaches the match, the pending penalty is
// drawn by itself when nothing can go on it, at the pace of the cards.

using namespace std::chrono_literals;

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::refusal;
using uno::testing::Table;

[[nodiscard]] RoomSettingsPatch ladder()
{
    RoomSettingsPatch patch;
    patch.stacking = core::PenaltyStacking::Ladder;
    return patch;
}

[[nodiscard]] bool holdsPenaltyCard(const core::Round& round, const core::PlayerId& who)
{
    const auto hand = round.hand(who).value_or(std::span<const core::Card>{});
    return std::ranges::any_of(hand, [](const core::Card& card) { return core::penaltyLevel(card.rank).has_value(); });
}

struct LadderTable {
    std::unique_ptr<Table> table;
    core::Card drawTwo;
    core::PlayerId poser;
    core::PlayerId target;
};

// A guided ladder room of three where the player on turn can play a Draw Two and the next player holds no penalty card.
LadderTable tableWithPlayableDrawTwo(Timeouts timeouts)
{
    for (std::uint64_t seed = 1; seed < 600; ++seed) {
        auto table = std::make_unique<Table>(3, seed, core::MatchLength::SingleRound, core::DrawRule::Guided, false,
                                             core::DrawAmount::One, timeouts);
        table->players.front().send(request::UpdateSettings{.settings = ladder()});
        table->start();
        const auto& round = table->room().match->round();
        const auto hand = round.hand(round.currentPlayer()).value_or(std::span<const core::Card>{});
        const auto drawTwo = std::ranges::find_if(hand, [&](const core::Card& card) {
            return card.rank == core::Rank::DrawTwo &&
                   core::isPlayable(card, round.discardPile().top(), round.currentColor());
        });
        const auto& seats = round.seats();
        const auto self = std::ranges::find(seats, round.currentPlayer()) - seats.begin();
        const auto& next = *std::next(seats.begin(), (self + 1) % static_cast<std::ptrdiff_t>(seats.size()));
        if (drawTwo == hand.end() || !std::holds_alternative<core::AwaitingPlay>(round.phase()) ||
            holdsPenaltyCard(round, next)) {
            continue;
        }
        auto found = LadderTable{.table = nullptr, .drawTwo = *drawTwo, .poser = round.currentPlayer(), .target = next};
        table->harness.clock.advanceMillis(60'000);
        table->clearInboxes();
        found.table = std::move(table);
        return found;
    }
    throw std::logic_error{"no seed gave the player on turn a playable Draw Two"};
}

} // namespace

TEST_CASE("The host chooses how penalties stack, and the match is played that way", "[app][stacking][settings]")
{
    Table table(3, 7, core::MatchLength::SingleRound);
    REQUIRE(table.room().settings.stacking == core::PenaltyStacking::Official);

    table.players.at(1).send(request::UpdateSettings{.settings = ladder()});
    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);
    REQUIRE(table.room().settings.stacking == core::PenaltyStacking::Official);

    table.players.front().send(request::UpdateSettings{.settings = ladder()});
    table.start();

    REQUIRE(table.room().settings.stacking == core::PenaltyStacking::Ladder);
    REQUIRE(table.room().match->round().stacking() == core::PenaltyStacking::Ladder);
}

TEST_CASE("Ladder: a Draw Two nobody can answer is drawn by the target after the effect and a short pause",
          "[app][stacking][pace]")
{
    auto [table, drawTwo, poser, target] = tableWithPlayableDrawTwo(Timeouts{});
    table->playerWithId(poser).send(request::PlayCard{
        .cardId = drawTwo.id,
        .chosenColor = std::nullopt,
        .swapTargetId = std::nullopt,
        .targetId = std::nullopt,
    });
    const auto& round = table->room().match->round();
    REQUIRE(std::holds_alternative<core::AwaitingStackResponse>(round.phase()));
    REQUIRE(round.currentPlayer() == target);
    const auto cardsBefore = round.hand(target)->size();
    // The card, the "+2" and the pause: the same as for any Draw Two
    REQUIRE(table->room().actionsOpenAt - table->harness.clock.nowMillis() == (1100ms + 1500ms).count());

    const auto opensAt = table->room().actionsOpenAt;
    table->harness.scheduler.advance(std::chrono::milliseconds(opensAt - table->harness.clock.nowMillis()) + 1199ms);
    REQUIRE(round.hand(target)->size() == cardsBefore);
    table->harness.scheduler.advance(1ms);

    REQUIRE(round.hand(target)->size() == cardsBefore + 2);
    REQUIRE(std::holds_alternative<core::AwaitingPlay>(round.phase()));
    REQUIRE(round.currentPlayer() != target);
}
