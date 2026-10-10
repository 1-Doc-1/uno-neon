#include "uno/app/bot_level.hpp"
#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/legal_actions.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// The bots through the application (ADR 0030): they sit in a room like players, play through the same checks, wait
// their turn to think, and never hold the room, the host role or the end of a round.

using namespace std::chrono_literals;

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::AppHarness;
using uno::testing::legalActionsOfCurrentPlayer;
using uno::testing::refusal;
using uno::testing::TestPlayer;
using uno::testing::toRequest;

RoomSettingsPatch singleRound()
{
    RoomSettingsPatch patch;
    patch.matchLength = core::MatchLength::SingleRound;
    return patch;
}

RoomCode createRoom(TestPlayer& host)
{
    host.send(request::CreateRoom{.nickname = "Alice", .settings = std::nullopt});
    return host.room().code;
}

Room& roomOf(AppHarness& harness, const RoomCode& code)
{
    Room* const room = harness.rooms.find(code);
    REQUIRE(room != nullptr);
    return *room;
}

std::size_t botCount(const response::RoomView& room)
{
    return static_cast<std::size_t>(std::ranges::count(room.players, true, &response::RoomMember::isBot));
}

} // namespace

TEST_CASE("The host adds a bot to the lobby: it is there, ready, and never the host", "[app][bots]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    static_cast<void>(host.received());

    host.send(request::AddBot{.level = BotLevel::Normal});

    const auto room = host.room();
    REQUIRE(room.players.size() == 2);
    const auto& bot = room.players.back();
    REQUIRE(bot.isBot);
    REQUIRE(bot.isReady);
    REQUIRE(bot.isConnected);
    REQUIRE_FALSE(bot.isHost);
    REQUIRE_FALSE(bot.nickname.empty());
    REQUIRE(roomOf(harness, code).find(bot.playerId)->botLevel == BotLevel::Normal);
}

TEST_CASE("Only the host adds bots, and not beyond the size of the room", "[app][bots]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    guest.send(request::JoinRoom{.code = code, .nickname = "Max"});
    static_cast<void>(guest.received());

    guest.send(request::AddBot{.level = BotLevel::Easy});
    REQUIRE(refusal(guest.received()) == ErrorCode::NotHost);

    RoomSettingsPatch patch;
    patch.maxPlayers = 3;
    host.send(request::UpdateSettings{.settings = patch});
    host.send(request::AddBot{.level = BotLevel::Easy});
    static_cast<void>(host.received());
    host.send(request::AddBot{.level = BotLevel::Easy});
    REQUIRE(refusal(host.received()) == ErrorCode::RoomFull);
    REQUIRE(botCount(host.room()) == 1);
}

TEST_CASE("The bots of a room have distinct names", "[app][bots]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    static_cast<void>(createRoom(host));
    for (int bot = 0; bot < 5; ++bot) {
        host.send(request::AddBot{.level = BotLevel::Easy});
    }

    const auto room = host.room();
    std::vector<std::string> names;
    names.reserve(room.players.size());
    for (const auto& member : room.players) {
        names.push_back(member.nickname);
    }
    std::ranges::sort(names);
    REQUIRE(std::ranges::adjacent_find(names) == names.end());
}

TEST_CASE("The host removes a bot with a kick, and starts without anybody having to be ready", "[app][bots]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    host.send(request::AddBot{.level = BotLevel::Easy});
    host.send(request::AddBot{.level = BotLevel::Normal});
    const auto removed = host.room().players.at(1).playerId;

    host.send(request::Kick{.playerId = removed});

    REQUIRE(host.room().players.size() == 2);
    REQUIRE(roomOf(harness, code).find(removed) == nullptr);
    host.send(request::StartMatch{});
    REQUIRE(roomOf(harness, code).phase == response::RoomPhase::InGame);
}

TEST_CASE("When the host leaves the lobby the role goes to a person, never to a bot", "[app][bots]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    host.send(request::AddBot{.level = BotLevel::Easy}); // seated right after the host
    auto guest = harness.helloPlayer();
    guest.send(request::JoinRoom{.code = code, .nickname = "Max"});

    host.send(request::LeaveRoom{});

    const Room& room = roomOf(harness, code);
    REQUIRE(room.host == guest.id());
    REQUIRE(room.members.size() == 2);
}

TEST_CASE("A game against bots starts at once: no waiting room, seats named and marked as bots", "[app][bots][solo]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();

    player.send(request::CreateBotGame{
        .nickname = "Alice",
        .botCount = 3,
        .level = BotLevel::Normal,
        .settings = singleRound(),
    });

    const auto update = player.last<response::GameUpdate>();
    REQUIRE(update.has_value());
    REQUIRE(update->view.seats.size() == 4);
    REQUIRE(std::ranges::count(update->view.seats, true, &response::SeatInfo::isBot) == 3);
    const auto room = player.room();
    REQUIRE(room.phase == response::RoomPhase::InGame);
    REQUIRE(room.settings.maxPlayers == 4);
    REQUIRE(room.settings.matchLength == core::MatchLength::SingleRound);
}

TEST_CASE("A game against bots takes one to five bots, and the host nickname is checked", "[app][bots][solo]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();

    player.send(
        request::CreateBotGame{.nickname = "Alice", .botCount = 0, .level = BotLevel::Easy, .settings = std::nullopt});
    REQUIRE(refusal(player.received()) == ErrorCode::InvalidSettings);
    player.send(
        request::CreateBotGame{.nickname = "Alice", .botCount = 6, .level = BotLevel::Easy, .settings = std::nullopt});
    REQUIRE(refusal(player.received()) == ErrorCode::InvalidSettings);
    player.send(
        request::CreateBotGame{.nickname = "A", .botCount = 2, .level = BotLevel::Easy, .settings = std::nullopt});
    REQUIRE(refusal(player.received()) == ErrorCode::NicknameInvalid);

    player.send(
        request::CreateBotGame{.nickname = "Alice", .botCount = 5, .level = BotLevel::Easy, .settings = std::nullopt});
    REQUIRE_FALSE(refusal(player.received()).has_value());
}

TEST_CASE("A player already in a room cannot start a game against bots", "[app][bots][solo]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();
    static_cast<void>(createRoom(player));
    static_cast<void>(player.received());

    player.send(
        request::CreateBotGame{.nickname = "Alice", .botCount = 2, .level = BotLevel::Easy, .settings = std::nullopt});

    REQUIRE(refusal(player.received()) == ErrorCode::AlreadyInRoom);
}

TEST_CASE("A whole round with one person and three bots is played to the end, whatever the level", "[app][bots][solo]")
{
    const auto level = GENERATE(BotLevel::Easy, BotLevel::Normal);
    AppHarness harness{11};
    auto human = harness.helloPlayer();
    human.send(request::CreateBotGame{.nickname = "Alice", .botCount = 3, .level = level, .settings = singleRound()});
    const auto code = human.room().code;
    Room& room = roomOf(harness, code);

    // The person plays the first legal move they have; the bots play themselves, on the clock of the scheduler
    int steps = 0;
    while (room.phase != response::RoomPhase::MatchOver && steps++ < 40'000) {
        harness.scheduler.advance(50ms);
        if (room.phase != response::RoomPhase::InGame || !room.match.has_value()) {
            continue;
        }
        const core::Round& round = room.match->round();
        if (round.currentPlayer() == human.id() && !std::holds_alternative<core::RoundOver>(round.phase())) {
            const auto legal = legalActionsOfCurrentPlayer(round);
            REQUIRE_FALSE(legal.empty());
            human.send(toRequest(legal.front()));
        }
    }

    REQUIRE(room.phase == response::RoomPhase::MatchOver);
    REQUIRE(room.match->winner().has_value());
}

TEST_CASE("A bot waits for its turn to think: not before a second, and no later than two after the effect",
          "[app][bots][pace]")
{
    // The real delays, with a seat where a bot plays first
    for (std::uint64_t seed = 1; seed < 100; ++seed) {
        AppHarness harness{seed, Timeouts{}};
        auto human = harness.helloPlayer();
        human.send(request::CreateBotGame{
            .nickname = "Alice",
            .botCount = 1,
            .level = BotLevel::Normal,
            .settings = singleRound(),
        });
        Room& room = roomOf(harness, human.room().code);
        if (room.match->round().currentPlayer() == human.id()) {
            continue;
        }
        const auto version = room.stateVersion;
        const auto openAt = room.actionsOpenAt - harness.clock.nowMillis();

        harness.scheduler.advance(std::chrono::milliseconds(openAt) + 999ms);
        REQUIRE(room.stateVersion == version);
        harness.scheduler.advance(1001ms);
        REQUIRE(room.stateVersion > version);
        return;
    }
    FAIL("no seed gave the first move to the bot");
}

TEST_CASE("The last person leaving closes the room: bots alone do not play on", "[app][bots][solo]")
{
    AppHarness harness;
    auto human = harness.helloPlayer();
    human.send(
        request::CreateBotGame{.nickname = "Alice", .botCount = 2, .level = BotLevel::Easy, .settings = std::nullopt});
    const auto code = human.room().code;

    human.send(request::LeaveRoom{});

    REQUIRE(harness.rooms.find(code) == nullptr);
    harness.scheduler.advance(60s); // no timer of the room is left to fire into nothing
}

TEST_CASE("Between two rounds only the people are waited for", "[app][bots]")
{
    AppHarness harness{5};
    auto human = harness.helloPlayer();
    RoomSettingsPatch patch;
    patch.matchLength = core::MatchLength::To500;
    human.send(
        request::CreateBotGame{.nickname = "Alice", .botCount = 2, .level = BotLevel::Normal, .settings = patch});
    Room& room = roomOf(harness, human.room().code);

    int steps = 0;
    while (!std::holds_alternative<core::RoundOver>(room.match->round().phase()) && steps++ < 40'000) {
        harness.scheduler.advance(50ms);
        const core::Round& round = room.match->round();
        if (round.currentPlayer() == human.id() && !std::holds_alternative<core::RoundOver>(round.phase())) {
            human.send(toRequest(legalActionsOfCurrentPlayer(round).front()));
        }
    }
    REQUIRE(std::holds_alternative<core::RoundOver>(room.match->round().phase()));
    REQUIRE_FALSE(room.match->winner().has_value());
    const auto roundNumber = room.match->roundNumber();

    human.send(request::ReadyForNextRound{});

    REQUIRE(room.match->roundNumber() == roundNumber + 1);
}
