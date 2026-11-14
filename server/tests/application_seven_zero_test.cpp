#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/card.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/match.hpp"
#include "uno/core/turn_phase.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <variant>

// The 7-0 house rule through the application (ADR 0041): the host's setting reaches the match, `swapTargetId` names the
// player a 7 swaps hands with, everybody is told of the exchange, and it takes its place in the pause that follows a
// play.

using namespace std::chrono_literals;

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::ofType;
using uno::testing::refusal;
using uno::testing::refusalReason;
using uno::testing::Table;

[[nodiscard]] RoomSettingsPatch sevenZero()
{
    RoomSettingsPatch patch;
    patch.sevenZero = true;
    return patch;
}

struct SevenTable {
    std::unique_ptr<Table> table;
    core::CardId seven;
    core::PlayerId poser;
    core::PlayerId target;
};

// A room of three with the 7-0 rule where the player on turn holds a 7 they can play. The target is the next seat.
[[nodiscard]] SevenTable tableWithPlayableSeven(Timeouts timeouts)
{
    for (std::uint64_t seed = 1; seed < 800; ++seed) {
        auto table = std::make_unique<Table>(3, seed, core::MatchLength::SingleRound, core::DrawRule::Guided, false,
                                             core::DrawAmount::One, timeouts);
        table->players.front().send(request::UpdateSettings{.settings = sevenZero()});
        table->start();
        const auto& round = table->room().match->round();
        if (!std::holds_alternative<core::AwaitingPlay>(round.phase())) {
            continue;
        }
        const auto hand = round.hand(round.currentPlayer()).value_or(std::span<const core::Card>{});
        const auto seven = std::ranges::find_if(hand, [&](const core::Card& card) {
            return card.rank == core::Rank::Seven && round.canPlayCard(round.currentPlayer(), card);
        });
        if (seven == hand.end()) {
            continue;
        }
        const auto& seats = round.seats();
        const auto self = std::ranges::find(seats, round.currentPlayer()) - seats.begin();
        const auto& next = *std::next(seats.begin(), (self + 1) % static_cast<std::ptrdiff_t>(seats.size()));
        auto found = SevenTable{.table = nullptr, .seven = seven->id, .poser = round.currentPlayer(), .target = next};
        table->harness.clock.advanceMillis(60'000);
        table->clearInboxes();
        found.table = std::move(table);
        return found;
    }
    throw std::logic_error{"no seed gave the player on turn a playable 7"};
}

[[nodiscard]] request::PlayCard playSeven(const core::CardId& card, std::optional<core::PlayerId> swapTarget,
                                          std::optional<core::PlayerId> target = std::nullopt)
{
    return request::PlayCard{
        .cardId = card,
        .chosenColor = std::nullopt,
        .swapTargetId = std::move(swapTarget),
        .targetId = std::move(target),
    };
}

} // namespace

TEST_CASE("The host chooses the 7-0 rule, and the match is played that way", "[app][sevenZero][settings]")
{
    Table table(3, 7);
    REQUIRE_FALSE(table.room().settings.sevenZero);

    table.players.at(1).send(request::UpdateSettings{.settings = sevenZero()});
    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);

    table.players.front().send(request::UpdateSettings{.settings = sevenZero()});
    table.start();

    REQUIRE(table.room().settings.sevenZero);
    REQUIRE(table.room().match->round().sevenZero());
}

TEST_CASE("A 7 played with swapTargetId swaps the hands, and everybody is told who swapped with whom",
          "[app][sevenZero][swap]")
{
    auto [table, seven, poser, target] = tableWithPlayableSeven(Timeouts{});
    const auto& round = table->room().match->round();
    const auto posersHand = round.hand(poser)->size();
    const auto targetsHand = round.hand(target)->size();

    table->playerWithId(poser).send(playSeven(seven, target));

    REQUIRE(round.hand(poser)->size() == targetsHand);
    REQUIRE(round.hand(target)->size() == posersHand - 1);
    for (auto& player : table->players) {
        const auto updates = ofType<response::GameUpdate>(player.received());
        REQUIRE(updates.size() == 1);
        const auto swapped = std::ranges::count_if(updates.front().events, [&](const core::ClientEvent& event) {
            const auto* hands = std::get_if<core::HandsSwappedEvent>(&event);
            return hands != nullptr && hands->playerId == poser && hands->targetId == target;
        });
        REQUIRE(swapped == 1);
    }
}

TEST_CASE("The exchange takes its pause like any effect: the card, then the swap", "[app][sevenZero][pace]")
{
    auto [table, seven, poser, target] = tableWithPlayableSeven(Timeouts{});

    table->playerWithId(poser).send(playSeven(seven, target));

    REQUIRE(table->room().actionsOpenAt - table->harness.clock.nowMillis() == (1100ms + 1500ms).count());
}

TEST_CASE("A 7 without a player to swap with, or with two targets, is refused and changes nothing",
          "[app][sevenZero][refused]")
{
    auto [table, seven, poser, target] = tableWithPlayableSeven(Timeouts{});
    auto& player = table->playerWithId(poser);
    const core::Round before = table->room().match->round();

    player.send(playSeven(seven, std::nullopt));
    const auto missing = player.received();
    REQUIRE(refusal(missing) == ErrorCode::IllegalMove);
    REQUIRE(refusalReason(missing) == IllegalMoveReason::SwapTargetRequired);

    player.send(playSeven(seven, poser));
    REQUIRE(refusalReason(player.received()) == IllegalMoveReason::SwapTargetInvalid);

    player.send(playSeven(seven, target, target));
    REQUIRE(refusalReason(player.received()) == IllegalMoveReason::TargetNotAllowed);

    REQUIRE(table->room().match->round() == before);
}
