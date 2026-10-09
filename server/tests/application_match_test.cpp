#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/card.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/net/codec.hpp"
#include "uno/testing/legal_actions.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include "support/require.hpp"
#include "support/wire_leak.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <variant>
#include <vector>

// Matches run by the application (step 2.4): start, play, broadcast and the anti-leak guarantee on every
// message that leaves the server, checked on the serialized JSON.

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::AppHarness;
using uno::testing::ofType;
using uno::testing::refusal;
using uno::testing::requireWireLeaksNothing;
using uno::testing::Table;
using uno::testing::TestPlayer;
using uno::testing::toRequest;
using Messages = std::vector<response::Message>;

// Serializing and reading back every update of a long match is slow: this checks one in `stride`, and the last. The
// simulation of the engine checks every step of thousands of matches; the sockets test checks every update of two.
void requireNoLeakInUpdates(const TestPlayer& player, std::size_t stride = 9)
{
    const auto updates = player.all<response::GameUpdate>();
    for (std::size_t index = 0; index < updates.size(); index += stride) {
        requireWireLeaksNothing(updates.at(index), player.id());
    }
    if (!updates.empty()) {
        requireWireLeaksNothing(updates.back(), player.id());
    }
}

} // namespace

TEST_CASE("Only the host starts a match, with two ready players at least", "[app][match]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});
    const auto code = host.room().code;
    auto guest = harness.helloPlayer();
    static_cast<void>(host.received());

    host.send(request::StartMatch{});
    REQUIRE(refusal(host.received()) == ErrorCode::NotEnoughPlayers);

    guest.send(request::JoinRoom{.code = code, .nickname = "Max"});
    static_cast<void>(host.received());
    host.send(request::StartMatch{});
    REQUIRE(refusal(host.received()) == ErrorCode::PlayersNotReady);

    guest.send(request::SetReady{.ready = true});
    static_cast<void>(guest.received());
    guest.send(request::StartMatch{});
    REQUIRE(refusal(guest.received()) == ErrorCode::NotHost);

    auto outsider = harness.helloPlayer();
    static_cast<void>(outsider.received());
    outsider.send(request::StartMatch{});
    REQUIRE(refusal(outsider.received()) == ErrorCode::NotInRoom);

    static_cast<void>(host.received());
    host.send(request::StartMatch{});
    REQUIRE_FALSE(refusal(host.received()).has_value());
    host.send(request::StartMatch{});
    REQUIRE(refusal(host.received()) == ErrorCode::MatchInProgress);
}

TEST_CASE("Starting a match deals every player their own hand, after the ack and the room update", "[app][match]")
{
    Table table(3);

    table.start();

    for (std::size_t seat = 0; seat < 3; ++seat) {
        auto& player = table.players.at(seat);
        const auto messages = player.received();
        if (seat == 0) {
            REQUIRE(std::holds_alternative<response::Ack>(messages.at(0)));
            REQUIRE(std::holds_alternative<response::RoomUpdate>(messages.at(1)));
            REQUIRE(std::holds_alternative<response::GameUpdate>(messages.at(2)));
        }
        const auto update = ofType<response::GameUpdate>(messages).front();
        REQUIRE(update.view.stateVersion == 1);
        REQUIRE(update.view.game.me.playerId == player.id());
        REQUIRE(update.view.game.me.hand.size() == 7);
        REQUIRE(update.view.game.players.size() == 3);
        REQUIRE(update.view.seats.at(seat).nickname == "Player" + std::to_string(seat));
        REQUIRE(update.view.seats.at(0).isHost);
        REQUIRE(update.view.game.round == 1);
        const auto started = std::get<core::RoundStartedEvent>(update.events.front());
        REQUIRE(started.round == 1);
    }
    REQUIRE(table.room().phase == response::RoomPhase::InGame);
}

TEST_CASE("Nobody joins or changes settings once the match has started", "[app][match]")
{
    Table table(3);
    table.start();
    auto late = table.harness.helloPlayer();
    static_cast<void>(late.received());

    late.send(request::JoinRoom{.code = table.code, .nickname = "Late"});
    REQUIRE(refusal(late.received()) == ErrorCode::MatchInProgress);
    table.players.front().send(request::UpdateSettings{.settings = Table::patch(core::MatchLength::To250)});
    REQUIRE(refusal(table.players.front().received()) == ErrorCode::MatchInProgress);
}

TEST_CASE("Match actions are refused outside a running match", "[app][match]")
{
    Table table(2);

    table.players.front().send(request::DrawCard{});
    REQUIRE(refusal(table.players.front().received()) == ErrorCode::InvalidPhase);
}

TEST_CASE("A move out of turn or with a card not held is refused and tells nobody", "[app][match]")
{
    Table table(3);
    table.start();
    table.clearInboxes();
    auto& current = table.currentPlayer();
    auto& other =
        *std::ranges::find_if(table.players, [&](const TestPlayer& player) { return player.id() != current.id(); });

    other.send(request::DrawCard{});
    REQUIRE(refusal(other.received()) == ErrorCode::NotYourTurn);
    current.send(request::PlayCard{.cardId = core::CardId{999999},
                                   .chosenColor = std::nullopt,
                                   .swapTargetId = std::nullopt,
                                   .targetId = std::nullopt});
    REQUIRE(refusal(current.received()) == ErrorCode::CardNotInHand);

    for (auto& player : table.players) {
        REQUIRE(ofType<response::GameUpdate>(player.received()).empty());
    }
}

TEST_CASE("Swapping hands is not available", "[app][match]")
{
    Table table(2);
    table.start();
    auto& current = table.currentPlayer();
    const auto card = current.last<response::GameUpdate>().value().view.game.me.hand.front().id;
    const auto target =
        table.players.front().id() == current.id() ? table.players.back().id() : table.players.front().id();
    static_cast<void>(current.received());

    current.send(request::PlayCard{
        .cardId = card, .chosenColor = std::nullopt, .swapTargetId = target, .targetId = std::nullopt});

    const auto replies = current.received();
    REQUIRE(refusal(replies) == ErrorCode::IllegalMove);
    REQUIRE(uno::testing::refusalReason(replies) == IllegalMoveReason::SwapTargetInvalid);
}

TEST_CASE("Every accepted action reaches every player with a consecutive state version", "[app][match]")
{
    Table table(3, 7, core::MatchLength::SingleRound, core::DrawRule::Official);
    table.start();
    table.clearInboxes();
    auto& current = table.currentPlayer();

    current.send(request::DrawCard{});

    std::set<std::uint64_t> versions;
    for (auto& player : table.players) {
        const auto updates = ofType<response::GameUpdate>(player.received());
        REQUIRE(updates.size() == 1);
        versions.insert(updates.front().view.stateVersion);
        const bool drewCards = std::ranges::any_of(updates.front().events, [](const core::ClientEvent& event) {
            return std::holds_alternative<core::CardsDrawnEvent>(event);
        });
        REQUIRE(drewCards);
    }
    REQUIRE(versions == std::set<std::uint64_t>{2});
}

TEST_CASE("A whole match played through requests never leaks a card on the wire", "[app][match][leak]")
{
    for (std::uint64_t seed = 1; seed <= 6; ++seed) {
        const auto count = 2 + (seed % 4); // 2 to 5 players
        Table table(count, seed);
        table.start();

        std::size_t steps = 0;
        while (table.room().phase == response::RoomPhase::InGame && steps < 4000) {
            ++steps;
            const auto& round = table.room().match->round();
            if (std::holds_alternative<core::RoundOver>(round.phase())) {
                break;
            }
            const auto actions = uno::testing::legalActionsOfCurrentPlayer(round);
            REQUIRE_FALSE(actions.empty());
            const auto& action = actions.at(steps % actions.size());
            auto& current = table.currentPlayer();
            static_cast<void>(current.received());
            current.send(toRequest(action));
            REQUIRE_FALSE(refusal(current.received()).has_value());
        }

        REQUIRE(std::holds_alternative<core::RoundOver>(table.room().match->round().phase()));
        for (auto& player : table.players) {
            requireNoLeakInUpdates(player);
        }
        // Once the round is over the remaining hands are public.
        const auto last = table.players.front().last<response::GameUpdate>().value();
        REQUIRE(last.view.game.roundResult.has_value());
        REQUIRE(last.view.game.roundResult->revealedHands.size() == count);
    }
}

TEST_CASE("The room moves to matchOver when a single round decides the match, and a rematch restarts it",
          "[app][match]")
{
    Table table(2, 3);
    table.start();
    while (table.room().phase == response::RoomPhase::InGame) {
        const auto actions = uno::testing::legalActionsOfCurrentPlayer(table.room().match->round());
        auto& current = table.currentPlayer();
        current.send(toRequest(actions.front()));
    }
    REQUIRE(table.room().match->winner().has_value());
    REQUIRE(table.room().phase == response::RoomPhase::MatchOver);
    const auto lastVersion = table.room().stateVersion;
    REQUIRE(table.players.front().last<response::RoomUpdate>().value().room.phase == response::RoomPhase::MatchOver);

    table.clearInboxes();
    table.players.back().send(request::Rematch{});
    REQUIRE(refusal(table.players.back().received()) == ErrorCode::NotHost);
    table.players.front().send(request::Rematch{});

    REQUIRE(table.room().phase == response::RoomPhase::InGame);
    const auto update = table.players.back().last<response::GameUpdate>().value();
    REQUIRE(update.view.stateVersion == lastVersion + 1);
    REQUIRE(update.view.game.round == 1);
    REQUIRE(std::ranges::all_of(update.view.game.players, [](const core::SeatView& seat) { return seat.score == 0; }));
}

TEST_CASE("The next round starts when every connected player is ready", "[app][match]")
{
    Table table(3, 5, core::MatchLength::To500);
    table.start();
    while (!std::holds_alternative<core::RoundOver>(table.room().match->round().phase())) {
        const auto actions = uno::testing::legalActionsOfCurrentPlayer(table.room().match->round());
        table.currentPlayer().send(toRequest(actions.front()));
    }
    REQUIRE(table.room().match->roundNumber() == 1);
    table.clearInboxes();

    table.players.at(0).send(request::ReadyForNextRound{});
    table.players.at(1).send(request::ReadyForNextRound{});
    REQUIRE(table.room().match->roundNumber() == 1);
    REQUIRE(table.players.at(2).last<response::GameUpdate>().value().view.seats.at(0).isReadyForNextRound);
    table.players.at(2).send(request::ReadyForNextRound{});

    REQUIRE(table.room().match->roundNumber() == 2);
    const auto update = table.players.at(2).last<response::GameUpdate>().value();
    REQUIRE(update.view.game.round == 2);
    REQUIRE_FALSE(update.view.seats.at(0).isReadyForNextRound);
    const auto started = std::get<core::RoundStartedEvent>(update.events.front());
    REQUIRE(started.round == 2);
    // The winner of round 1 scored; nobody else did.
    REQUIRE(std::ranges::count_if(update.view.game.players,
                                  [](const core::SeatView& seat) { return seat.score > 0; }) == 1);
}

TEST_CASE("Ready for the next round is refused while a round is being played", "[app][match]")
{
    Table table(2);
    table.start();
    table.clearInboxes();

    table.players.front().send(request::ReadyForNextRound{});

    REQUIRE(refusal(table.players.front().received()) == ErrorCode::InvalidPhase);
}

TEST_CASE("A disconnection during a match is announced, and the return resynchronises the player", "[app][match]")
{
    Table table(3);
    table.start();
    table.clearInboxes();
    const auto& leaver = table.players.at(2);

    table.harness.application.onDisconnected(leaver.connection());

    const auto update = table.players.at(0).last<response::GameUpdate>().value();
    REQUIRE(std::holds_alternative<core::PlayerDisconnectedEvent>(update.events.back()));
    REQUIRE_FALSE(update.view.seats.at(2).isConnected);

    auto back = table.harness.connect();
    back.hello(leaver.token());
    const auto resync = back.last<response::GameUpdate>().value();
    REQUIRE(resync.view.game.me.playerId == leaver.id());
    REQUIRE(resync.view.game.me.hand.size() >= 7);
    REQUIRE(resync.view.stateVersion == update.view.stateVersion + 1);
    REQUIRE(back.last<response::Welcome>().value().resumedRoomCode == table.code);
    const auto announced = table.players.at(0).last<response::GameUpdate>().value();
    REQUIRE(std::holds_alternative<core::PlayerReconnectedEvent>(announced.events.back()));
    requireWireLeaksNothing(resync, leaver.id());
}
