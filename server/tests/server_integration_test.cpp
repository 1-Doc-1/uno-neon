#include "uno/app/application.hpp"
#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/room_repository.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"
#include "uno/net/crypto_random_source.hpp"
#include "uno/net/crypto_runtime.hpp"
#include "uno/net/server_message_sink.hpp"
#include "uno/net/system_clock.hpp"
#include "uno/net/uws_scheduler.hpp"
#include "uno/net/websocket_server.hpp"

#include "support/require.hpp"
#include "support/running_server.hpp"
#include "support/wire_leak.hpp"
#include "support/ws_client.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// The whole server, as a client sees it (step 2.6): a real WebSocket server on a real port, the production wiring
// (cryptographic randomness, system clock, uWebSockets timers), and clients that speak the JSON protocol.

namespace {

namespace core = uno::core;
using namespace uno::app;
using namespace std::chrono_literals;
using uno::testing::require;
using uno::testing::WsClient;

constexpr const char* kOrigin = "http://localhost:4200";
constexpr std::size_t kStepLimit = 3000;

uno::net::WebSocketServerConfig configFor(const InMemoryRoomRepository& rooms, uno::net::UwsScheduler& scheduler)
{
    uno::net::WebSocketServerConfig config;
    config.originPolicy = uno::net::OriginPolicy({kOrigin});
    config.roomCount = [&rooms] { return rooms.size(); };
    config.onStop = [&scheduler] { scheduler.cancelAll(); };
    // Bots play as fast as the server answers: far above what the limits of a human player allow.
    config.rateLimits.burst = 10000;
    config.rateLimits.perSecond = 10000;
    return config;
}

// The production wiring, on a free port.
struct Deployment {
    // `timeouts` lets a test shorten the delays of the game (grace period, inactivity...) to milliseconds: a test
    // never waits for a real minute.
    explicit Deployment(Timeouts timeouts = {})
        : application(sink, rooms, random, clock, scheduler, timeouts),
          server(configFor(rooms, scheduler), application,
                 [this](uno::net::WebSocketServer& running) { sink.attach(running); })
    {
    }

    [[nodiscard]] WsClient client() const { return {server.port(), kOrigin}; }

    uno::net::CryptoRandomSource random;
    uno::net::SystemClock clock;
    uno::net::UwsScheduler scheduler;
    InMemoryRoomRepository rooms;
    uno::net::ServerMessageSink sink;
    Application application;
    uno::testing::RunningServer server;
};

// A client that said hello.
struct Player {
    WsClient client;
    std::string nickname;
    core::PlayerId id;
    SessionToken token;
    std::optional<response::GameView> view; // the latest the server sent
};

Player enter(const Deployment& deployment, const std::string& nickname)
{
    WsClient client = deployment.client();
    static_cast<void>(client.send(request::Hello{.sessionToken = std::nullopt, .clientVersion = "test"}));
    const auto welcome = require(client.await<response::Welcome>(), "a welcome");
    return Player{
        .client = std::move(client),
        .nickname = nickname,
        .id = welcome.playerId,
        .token = welcome.sessionToken,
        .view = std::nullopt,
    };
}

RoomCode openRoom(Player& host)
{
    host.client.sendAndExpectAck(request::CreateRoom{.nickname = host.nickname, .settings = std::nullopt});
    return require(host.client.await<response::RoomUpdate>(), "a room update").room.code;
}

void joinReady(Player& guest, const RoomCode& code)
{
    guest.client.sendAndExpectAck(request::JoinRoom{.code = code, .nickname = guest.nickname});
    guest.client.sendAndExpectAck(request::SetReady{.ready = true});
}

// Waits until the player has been sent a game.update newer than `afterVersion`, checks it leaks nothing, keeps its
// view.
void awaitUpdate(Player& player, std::uint64_t afterVersion)
{
    while (true) {
        const auto update = require(player.client.await<response::GameUpdate>(), "a game update");
        uno::testing::requireWireLeaksNothing(update, player.id);
        player.view = update.view;
        if (update.view.stateVersion > afterVersion) {
            return;
        }
    }
}

// What a client does from its view alone, as the Angular one will: the server computed everything it may do.
std::optional<request::Body> decide(const response::GameView& view)
{
    const auto& me = view.game.me;
    if (me.penaltyResponse) {
        return request::RespondPenalty{.response = core::PenaltyResponse::Accept};
    }
    if (me.canChooseColor) {
        return request::ChooseColor{.color = core::Color::Red};
    }
    if (view.game.currentPlayerId != me.playerId) {
        return std::nullopt;
    }
    if (!me.playableCardIds.empty()) {
        const auto card = *std::ranges::find(me.hand, me.playableCardIds.front(), &core::Card::id);
        return request::PlayCard{
            .cardId = card.id,
            .chosenColor = card.color ? std::nullopt : std::optional<core::Color>(core::Color::Red),
            .swapTargetId = std::nullopt,
        };
    }
    if (me.canDraw) {
        return request::DrawCard{};
    }
    if (me.canKeepDrawnCard) {
        return request::Pass{};
    }
    return std::nullopt;
}

// Plays the match to its end through the sockets only.
void playToTheEnd(std::vector<Player>& players)
{
    for (auto& player : players) {
        awaitUpdate(player, 0);
    }
    for (std::size_t step = 0; step < kStepLimit; ++step) {
        if (players.front().view->game.phase == core::ViewPhase::MatchOver) {
            return;
        }
        const auto version = players.front().view->stateVersion;
        bool acted = false;
        for (auto& player : players) {
            if (const auto action = decide(*player.view)) {
                player.client.sendAndExpectAck(*action);
                acted = true;
                break;
            }
        }
        REQUIRE(acted);
        for (auto& player : players) {
            awaitUpdate(player, version);
        }
    }
    FAIL("the match did not end");
}

} // namespace

TEST_CASE("Two players play a whole match through real sockets, and no card ever leaks", "[server][integration]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    const Deployment deployment;
    std::vector<Player> players;
    players.push_back(enter(deployment, "Alice"));
    players.push_back(enter(deployment, "Bob"));
    const auto code = openRoom(players.at(0));
    joinReady(players.at(1), code);
    players.at(0).client.sendAndExpectAck(request::UpdateSettings{
        .settings =
            [] {
                RoomSettingsPatch patch;
                patch.matchLength = core::MatchLength::SingleRound;
                return patch;
            }(),
    });
    players.at(0).client.sendAndExpectAck(request::StartMatch{});

    playToTheEnd(players);

    for (const auto& player : players) {
        REQUIRE(player.view->game.matchWinnerId.has_value());
        REQUIRE(player.view->game.roundResult.has_value());
    }
    // The winner emptied their hand; the other hand is public now.
    REQUIRE(players.at(0).view->game.roundResult->revealedHands.size() == 2);
}

TEST_CASE("Three players play a whole match too", "[server][integration]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    const Deployment deployment;
    std::vector<Player> players;
    for (const char* name : {"Alice", "Bob", "Chloé"}) {
        players.push_back(enter(deployment, name));
    }
    const auto code = openRoom(players.at(0));
    joinReady(players.at(1), code);
    joinReady(players.at(2), code);
    players.at(0).client.sendAndExpectAck(request::UpdateSettings{
        .settings =
            [] {
                RoomSettingsPatch patch;
                patch.matchLength = core::MatchLength::SingleRound;
                return patch;
            }(),
    });
    players.at(0).client.sendAndExpectAck(request::StartMatch{});

    playToTheEnd(players);

    REQUIRE(players.at(2).view->game.matchWinnerId.has_value());
}

TEST_CASE("A player who reconnects with their token gets their room and their hand back", "[server][integration]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    const Deployment deployment;
    std::vector<Player> players;
    players.push_back(enter(deployment, "Alice"));
    players.push_back(enter(deployment, "Bob"));
    const auto code = openRoom(players.at(0));
    joinReady(players.at(1), code);
    players.at(0).client.sendAndExpectAck(request::StartMatch{});
    for (auto& player : players) {
        awaitUpdate(player, 0);
    }
    const auto hand = players.at(1).view->game.me.hand;

    WsClient again = deployment.client();
    static_cast<void>(again.send(request::Hello{.sessionToken = players.at(1).token, .clientVersion = "test"}));

    const auto welcome = require(again.await<response::Welcome>(), "a welcome");
    REQUIRE(welcome.playerId == players.at(1).id);
    REQUIRE(welcome.resumedRoomCode == code);
    const auto resync = require(again.await<response::GameUpdate>(), "a game update");
    REQUIRE(resync.view.game.me.hand == hand);
    uno::testing::requireWireLeaksNothing(resync, players.at(1).id);
    // The previous connection of that session was closed by the server.
    const auto closure = require(players.at(1).client.socket().waitForClosure(), "a closure");
    REQUIRE(closure.code == 4000);
}

TEST_CASE("A closed connection is shown to the room as disconnected", "[server][integration]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    const Deployment deployment;
    Player host = enter(deployment, "Alice");
    const auto code = openRoom(host);
    {
        Player guest = enter(deployment, "Bob");
        joinReady(guest, code);
        static_cast<void>(require(host.client.await<response::RoomUpdate>()));
    } // the guest's socket closes here

    std::optional<response::RoomUpdate> latest;
    while (const auto update = host.client.await<response::RoomUpdate>(2s)) {
        latest = update;
        if (!update->room.players.at(1).isConnected) {
            break;
        }
    }

    const auto room = require(latest, "a room update");
    REQUIRE_FALSE(room.room.players.at(1).isConnected);
}

TEST_CASE("Requests before hello are refused, and the health endpoint counts rooms", "[server][integration]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    const Deployment deployment;
    WsClient anonymous = deployment.client();
    const auto id = anonymous.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});

    const auto answer = require(anonymous.answerTo(id), "an answer");

    REQUIRE(std::get<response::Error>(answer).code == ErrorCode::SessionRequired);
    Player host = enter(deployment, "Alice");
    static_cast<void>(openRoom(host));
    const auto health = require(uno::testing::httpGet(deployment.server.port(), "/health"), "a health answer");
    REQUIRE(health.body.contains("\"rooms\":1"));
}

TEST_CASE("The grace period runs on the real timers of the server, set to milliseconds", "[server][integration]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    Timeouts fast;
    fast.reconnectGrace = 150ms;
    const Deployment deployment(fast);
    Player host = enter(deployment, "Alice");
    const auto code = openRoom(host);
    {
        Player guest = enter(deployment, "Bob");
        joinReady(guest, code);
    } // the guest's socket closes here, and never comes back

    std::size_t playersLeft = 2;
    while (playersLeft > 1) {
        const auto update = require(host.client.await<response::RoomUpdate>(2s), "the removal of the guest");
        playersLeft = update.room.players.size();
    }

    REQUIRE(playersLeft == 1);
}
