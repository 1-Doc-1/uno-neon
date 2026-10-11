#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <set>
#include <string>

// What players do to each other around the match (ADR 0036, 0037): avatars, chat, going back to the lobby, being
// kicked from a match, and the limits that keep one visitor from using up the server.

using namespace std::chrono_literals;

namespace {

using namespace uno::app;
using uno::testing::AppHarness;
using uno::testing::ofType;
using uno::testing::refusal;
using uno::testing::Table;
using uno::testing::TestPlayer;

std::string chatTextOf(TestPlayer& player)
{
    const auto chat = player.last<response::ChatMessage>();
    return chat ? chat->text : std::string{};
}

} // namespace

// ---- avatars ----

TEST_CASE("Each player's avatar is shown to the whole room", "[app][avatar]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    auto guest = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt, .avatarId = 4});
    guest.send(request::JoinRoom{.code = host.room().code, .nickname = "Max", .avatarId = 9});

    const auto players = host.room().players;

    REQUIRE(players.at(0).avatarId == 4);
    REQUIRE(players.at(1).avatarId == 9);
    REQUIRE(guest.room().players.at(1).avatarId == 9);
}

TEST_CASE("Bots wear avatars nobody in the room wears", "[app][avatar]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt, .avatarId = 0});
    for (int bot = 0; bot < 5; ++bot) {
        host.send(request::AddBot{.level = BotLevel::Easy});
    }

    std::set<std::uint8_t> avatars;
    for (const auto& member : host.room().players) {
        avatars.insert(member.avatarId);
        REQUIRE(member.avatarId < kAvatarCount);
    }

    REQUIRE(avatars.size() == 6);
}

// ---- chat ----

TEST_CASE("A chat line reaches everybody in the room, the sender included, as plain text", "[app][chat]")
{
    Table table(3);

    table.players.at(1).send(request::SendChat{.text = "  Salut <b>tout</b> le monde !  "});

    for (auto& player : table.players) {
        const auto chat = player.last<response::ChatMessage>();
        REQUIRE(chat.has_value());
        REQUIRE(chat->playerId == table.players.at(1).id());
        REQUIRE(chat->text == "Salut <b>tout</b> le monde !"); // trimmed, never interpreted
    }
}

TEST_CASE("Chat works during the match too, and only for the room", "[app][chat]")
{
    Table table(3);
    table.start();
    auto stranger = table.harness.helloPlayer();
    table.clearInboxes();

    table.players.at(2).send(request::SendChat{.text = "gg"});
    stranger.send(request::SendChat{.text = "intrus"});

    REQUIRE(chatTextOf(table.players.at(0)) == "gg");
    REQUIRE(chatTextOf(stranger).empty());
    REQUIRE(refusal(stranger.received()) == ErrorCode::NotInRoom);
}

TEST_CASE("A chat message that is empty, too long or unreadable is refused", "[app][chat]")
{
    Table table(2);
    auto& sender = table.players.at(1);

    SECTION("empty, or only spaces")
    {
        sender.send(request::SendChat{.text = "   "});
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
    }
    SECTION("201 characters")
    {
        sender.send(request::SendChat{.text = std::string(201, 'a')});
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
    }
    SECTION("200 characters is the limit")
    {
        sender.send(request::SendChat{.text = std::string(200, 'a')});
        REQUIRE_FALSE(refusal(sender.received()).has_value());
    }
    SECTION("a line break or any control character")
    {
        sender.send(request::SendChat{.text = "ligne un\nligne deux"});
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
        sender.send(request::SendChat{.text = std::string("a\0b", 3)});
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
    }
    SECTION("text that is not UTF-8")
    {
        sender.send(request::SendChat{.text = "\xC3\x28"});
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
    }
    SECTION("invisible or direction-changing characters")
    {
        const std::string override = std::string("a") + "\xE2\x80\xAE" + "b"; // right-to-left override
        sender.send(request::SendChat{.text = override});
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
        sender.send(request::SendChat{.text = std::string("a") + "\xE2\x80\x8B" + "b"}); // zero-width space
        REQUIRE(refusal(sender.received()) == ErrorCode::ChatInvalid);
    }
    SECTION("an emoji and accents are fine")
    {
        sender.send(request::SendChat{.text = "Bien joué ! 🎉"});
        REQUIRE_FALSE(refusal(sender.received()).has_value());
    }
    REQUIRE(table.players.at(0).all<response::ChatMessage>().size() <= 1);
}

TEST_CASE("Chat is limited to five messages in ten seconds, per player", "[app][chat][rate]")
{
    Table table(2);
    auto& sender = table.players.at(0);

    for (int index = 0; index < 5; ++index) {
        sender.send(request::SendChat{.text = "message " + std::to_string(index)});
        REQUIRE_FALSE(refusal(sender.received()).has_value());
    }
    sender.send(request::SendChat{.text = "trop"});
    REQUIRE(refusal(sender.received()) == ErrorCode::RateLimited);
    // The other player has their own allowance
    table.players.at(1).send(request::SendChat{.text = "moi je peux"});
    REQUIRE_FALSE(refusal(table.players.at(1).received()).has_value());

    table.harness.clock.advanceMillis(10'001);
    sender.send(request::SendChat{.text = "de nouveau"});
    REQUIRE_FALSE(refusal(sender.received()).has_value());
}

TEST_CASE("A bot never writes: only people have a session to write from", "[app][chat]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateBotGame{
        .nickname = "Léa",
        .botCount = 2,
        .level = BotLevel::Easy,
        .settings = std::nullopt,
    });
    harness.scheduler.advance(10min);

    REQUIRE(host.all<response::ChatMessage>().empty());
}

// ---- back to the lobby ----

TEST_CASE("Only the host sends the room back to the lobby", "[app][lobby]")
{
    Table table(3);
    table.start();

    table.players.at(1).send(request::BackToLobby{});

    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);
    REQUIRE(table.room().phase == response::RoomPhase::InGame);
}

TEST_CASE("Going back to the lobby abandons the match: nobody is ready, and a new one can start", "[app][lobby]")
{
    Table table(3);
    table.start();
    REQUIRE(table.room().match.has_value());
    table.clearInboxes();

    table.players.front().send(request::BackToLobby{});

    REQUIRE(table.room().phase == response::RoomPhase::Lobby);
    REQUIRE_FALSE(table.room().match.has_value());
    for (auto& player : table.players) {
        const auto room = player.room();
        REQUIRE(room.phase == response::RoomPhase::Lobby);
        REQUIRE(room.players.size() == 3);
        for (const auto& member : room.players) {
            REQUIRE((member.isHost || !member.isReady));
        }
    }
    // The same people, the same settings: ready up, start again
    table.players.at(1).send(request::SetReady{.ready = true});
    table.players.at(2).send(request::SetReady{.ready = true});
    table.start();
    REQUIRE(table.room().phase == response::RoomPhase::InGame);
}

TEST_CASE("The host leaves a finished match for the lobby, and a lobby has nothing to go back from", "[app][lobby]")
{
    Table table(2);
    table.players.front().send(request::BackToLobby{});
    REQUIRE(refusal(table.players.front().received()) == ErrorCode::InvalidPhase);

    table.start();
    table.room().phase = response::RoomPhase::MatchOver; // as the engine leaves it when somebody wins
    table.players.front().send(request::BackToLobby{});

    REQUIRE(table.room().phase == response::RoomPhase::Lobby);
}

TEST_CASE("Nothing of the abandoned match survives: its timers do not play for anybody", "[app][lobby]")
{
    Table table(2);
    table.start();
    table.players.front().send(request::BackToLobby{});
    table.clearInboxes();

    table.harness.scheduler.advance(10min);

    for (auto& player : table.players) {
        REQUIRE(ofType<response::GameUpdate>(player.received()).empty());
    }
}

// ---- kicking during a match ----

TEST_CASE("The host kicks a player out of a running match: they forfeit like somebody who left", "[app][kick]")
{
    Table table(3);
    table.start();
    const auto kicked = table.players.at(2).id();
    table.clearInboxes();

    table.players.front().send(request::Kick{.playerId = kicked});

    REQUIRE(ofType<response::RoomClosed>(table.players.at(2).received()).size() == 1);
    REQUIRE(table.room().members.size() == 2);
    REQUIRE(table.room().match->round().seats().size() == 2);
    const auto update = table.players.at(1).last<response::GameUpdate>();
    REQUIRE(update.has_value());
    REQUIRE(update->view.game.players.size() == 2);
}

TEST_CASE("A player who is not the host cannot kick anybody, in the lobby or in a match", "[app][kick]")
{
    Table table(3);
    table.players.at(1).send(request::Kick{.playerId = table.players.at(2).id()});
    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);

    table.start();
    table.players.at(1).send(request::Kick{.playerId = table.players.at(2).id()});

    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);
    REQUIRE(table.room().members.size() == 3);
}

TEST_CASE("The host cannot kick themselves, in a match either", "[app][kick]")
{
    Table table(2);
    table.start();

    table.players.front().send(request::Kick{.playerId = table.players.front().id()});

    REQUIRE(refusal(table.players.front().received()) == ErrorCode::CannotKickSelf);
}

// ---- limits ----

TEST_CASE("One session is in one room at a time", "[app][limits]")
{
    Table table(2);
    auto& player = table.players.at(1);

    player.send(request::CreateRoom{.nickname = "Autre", .settings = std::nullopt});
    REQUIRE(refusal(player.received()) == ErrorCode::AlreadyInRoom);
    player.send(request::JoinRoom{.code = table.code, .nickname = "Autre"});
    REQUIRE(refusal(player.received()) == ErrorCode::AlreadyInRoom);
}

TEST_CASE("The server refuses new rooms beyond its limit, and takes them again once rooms close", "[app][limits]")
{
    AppHarness harness(7, uno::testing::withoutPresentationDelay(), Limits{.maxRooms = 2, .maxSessions = 100});
    auto first = harness.helloPlayer();
    auto second = harness.helloPlayer();
    auto third = harness.helloPlayer();
    first.send(request::CreateRoom{.nickname = "Un", .settings = std::nullopt});
    second.send(request::CreateRoom{.nickname = "Deux", .settings = std::nullopt});
    static_cast<void>(third.received());

    third.send(request::CreateRoom{.nickname = "Trois", .settings = std::nullopt});
    REQUIRE(refusal(third.received()) == ErrorCode::ServerFull);
    third.send(request::CreateBotGame{
        .nickname = "Trois",
        .botCount = 1,
        .level = BotLevel::Easy,
        .settings = std::nullopt,
    });
    REQUIRE(refusal(third.received()) == ErrorCode::ServerFull);

    first.send(request::LeaveRoom{});
    third.send(request::CreateRoom{.nickname = "Trois", .settings = std::nullopt});
    REQUIRE_FALSE(refusal(third.received()).has_value());
}

TEST_CASE("The server refuses new sessions beyond its limit", "[app][limits]")
{
    AppHarness harness(7, uno::testing::withoutPresentationDelay(), Limits{.maxRooms = 10, .maxSessions = 2});
    auto first = harness.helloPlayer();
    auto second = harness.helloPlayer();
    auto third = harness.connect();

    third.hello();

    REQUIRE(refusal(third.received()) == ErrorCode::ServerFull);
    // A returning player is not a new session
    harness.application.onDisconnected(first.connection());
    auto returning = harness.connect();
    returning.hello(first.token());
    REQUIRE(returning.last<response::Welcome>().has_value());
}

TEST_CASE("A room whose last person left is cleaned up, and a room abandoned by everybody too", "[app][limits]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});
    host.send(request::LeaveRoom{});
    REQUIRE(harness.rooms.size() == 0);

    auto lonely = harness.helloPlayer();
    lonely.send(request::CreateRoom{.nickname = "Max", .settings = std::nullopt});
    harness.application.onDisconnected(lonely.connection());
    harness.scheduler.advance(61s); // the grace period of a lost connection

    REQUIRE(harness.rooms.size() == 0);
}
