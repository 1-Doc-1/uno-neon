#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/draw_rule.hpp"

#include "support/app_harness.hpp"
#include "support/require.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

// The sessions and rooms of the application (step 2.3), driven without any network.

namespace {

using namespace uno::app;
using uno::testing::AppHarness;
using uno::testing::TestPlayer;

using Messages = std::vector<response::Message>;

template <typename T>
std::vector<T> ofType(const Messages& messages)
{
    std::vector<T> found;
    for (const auto& message : messages) {
        if (const auto* typed = std::get_if<T>(&message)) {
            found.push_back(*typed);
        }
    }
    return found;
}

// The error code a request was refused with, or nothing when it was acknowledged.
std::optional<ErrorCode> refusal(const Messages& messages)
{
    const auto errors = ofType<response::Error>(messages);
    return errors.empty() ? std::nullopt : std::optional<ErrorCode>(errors.front().code);
}

bool acknowledged(const Messages& messages)
{
    return !ofType<response::Ack>(messages).empty();
}

template <typename Setup>
RoomSettingsPatch patchOf(Setup setup)
{
    RoomSettingsPatch patch;
    setup(patch);
    return patch;
}

RoomCode createRoom(TestPlayer& host, const std::string& nickname = "Léa")
{
    host.send(request::CreateRoom{.nickname = nickname, .settings = std::nullopt});
    return host.room().code;
}

void join(TestPlayer& player, const RoomCode& code, const std::string& nickname)
{
    player.send(request::JoinRoom{.code = code, .nickname = nickname});
}

constexpr std::string_view kCodeAlphabet = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";

bool isUrlSafe(const std::string& text)
{
    return std::ranges::all_of(text, [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '-' || character == '_';
    });
}

} // namespace

TEST_CASE("Hello without a token creates a session and answers with a welcome only", "[app][session]")
{
    AppHarness harness;
    auto player = harness.connect();

    player.send(request::Hello{.sessionToken = std::nullopt, .clientVersion = "1.0.0"});
    const auto messages = player.received();

    REQUIRE(messages.size() == 1);
    const auto& welcome = std::get<response::Welcome>(messages.front());
    REQUIRE(welcome.sessionToken.value.size() == 22);
    REQUIRE(isUrlSafe(welcome.sessionToken.value));
    REQUIRE(welcome.playerId.value.starts_with("p_"));
    REQUIRE(welcome.playerId.value.size() >= 8);
    REQUIRE_FALSE(welcome.resumedRoomCode.has_value());
}

TEST_CASE("Two sessions never share a token or a player id", "[app][session]")
{
    AppHarness harness;

    const auto first = harness.helloPlayer();
    const auto second = harness.helloPlayer();

    REQUIRE(first.token() != second.token());
    REQUIRE(first.id() != second.id());
}

TEST_CASE("Any request before hello is refused with SESSION_REQUIRED", "[app][session]")
{
    AppHarness harness;
    auto player = harness.connect();

    player.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});

    REQUIRE(refusal(player.received()) == ErrorCode::SessionRequired);
}

TEST_CASE("A second hello on the same connection is refused", "[app][session]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();
    static_cast<void>(player.received());

    player.send(request::Hello{.sessionToken = std::nullopt, .clientVersion = "test"});

    REQUIRE(refusal(player.received()) == ErrorCode::InvalidPhase);
}

TEST_CASE("Hello with an unknown token is refused with SESSION_EXPIRED", "[app][session]")
{
    AppHarness harness;
    auto player = harness.connect();

    player.send(request::Hello{.sessionToken = SessionToken{"AAAAAAAAAAAAAAAAAAAAAA"}, .clientVersion = "test"});

    REQUIRE(refusal(player.received()) == ErrorCode::SessionExpired);
}

TEST_CASE("Hello with a known token resumes the session and its room", "[app][session]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    host.send(request::SetReady{.ready = true});

    auto returning = harness.connect();
    returning.hello(host.token());

    const auto welcome = uno::testing::require(returning.last<response::Welcome>());
    REQUIRE(welcome.playerId == host.id());
    REQUIRE(welcome.resumedRoomCode == code);
    REQUIRE(returning.room().players.size() == 1);
}

TEST_CASE("The newest connection of a session wins and the old one is closed", "[app][session]")
{
    AppHarness harness;
    auto first = harness.helloPlayer();
    const auto code = createRoom(first);

    auto second = harness.connect();
    second.hello(first.token());
    harness.application.onDisconnected(first.connection()); // the old socket finally notices it is dead

    REQUIRE(harness.sink.closed.size() == 1);
    REQUIRE(harness.sink.closed.front().connection == first.connection());
    // The late disconnection of the old socket must not mark the player as gone.
    const auto member = second.room().players.front();
    REQUIRE(member.isConnected);
    REQUIRE(second.room().code == code);
}

TEST_CASE("Creating a room makes the creator its host and acknowledges before updating", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    static_cast<void>(host.received());

    host.send(request::CreateRoom{.nickname = "  Léa ", .settings = std::nullopt});
    const auto messages = host.received();

    REQUIRE(messages.size() == 2);
    REQUIRE(std::holds_alternative<response::Ack>(messages.at(0)));
    const auto& room = std::get<response::RoomUpdate>(messages.at(1)).room;
    REQUIRE(room.code.value.size() == 6);
    REQUIRE(room.phase == response::RoomPhase::Lobby);
    REQUIRE(room.settings == RoomSettings{});
    REQUIRE(room.players.size() == 1);
    REQUIRE(room.players.front().nickname == "Léa");
    REQUIRE(room.players.front().isHost);
    REQUIRE(room.players.front().isReady);
    REQUIRE(harness.rooms.size() == 1);
}

TEST_CASE("Room codes only use the unambiguous alphabet", "[app][room]")
{
    AppHarness harness;
    for (int index = 0; index < 50; ++index) {
        auto host = harness.helloPlayer();
        const auto code = createRoom(host);
        REQUIRE(std::ranges::all_of(code.value, [](char character) { return kCodeAlphabet.contains(character); }));
    }
    REQUIRE(harness.rooms.size() == 50);
}

TEST_CASE("A nickname that breaks the rules is refused", "[app][room]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();

    for (const std::string nickname :
         {"", "a", " a ", "ABCDEFGHIJKLMNOPQ", "<b>Léa</b>", "Lé\"a", "Léa😀", "Lé\ta", "Lé\xCC\x81"}) {
        static_cast<void>(player.received());
        player.send(request::CreateRoom{.nickname = nickname, .settings = std::nullopt});

        INFO(nickname);
        REQUIRE(refusal(player.received()) == ErrorCode::NicknameInvalid);
    }
    REQUIRE(harness.rooms.size() == 0);
}

TEST_CASE("Letters of other alphabets, digits, spaces, underscores and hyphens are fine", "[app][room]")
{
    AppHarness harness;

    for (const std::string nickname : {"Zoé", "Ünal", "Ωmega", "Мария", "山田太郎", "Player_1", "a-b c", "12"}) {
        auto player = harness.helloPlayer();

        player.send(request::CreateRoom{.nickname = nickname, .settings = std::nullopt});

        INFO(nickname);
        REQUIRE(acknowledged(player.received()));
    }
}

TEST_CASE("A player already in a room cannot create or join another", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    static_cast<void>(host.received());

    host.send(request::CreateRoom{.nickname = "Léa", .settings = std::nullopt});
    REQUIRE(refusal(host.received()) == ErrorCode::AlreadyInRoom);
    join(host, code, "Léa");
    REQUIRE(refusal(host.received()) == ErrorCode::AlreadyInRoom);
}

TEST_CASE("Settings given at creation are applied, house rules are refused for now", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    static_cast<void>(host.received());

    host.send(request::CreateRoom{
        .nickname = "Léa",
        .settings = patchOf([](RoomSettingsPatch& patch) { patch.jumpIn = true; }),
    });
    REQUIRE(refusal(host.received()) == ErrorCode::InvalidSettings);
    host.send(request::CreateRoom{
        .nickname = "Léa",
        .settings = patchOf([](RoomSettingsPatch& patch) {
            patch.turnTimer = TurnTimerSeconds::Sixty;
            patch.matchLength = uno::core::MatchLength::SingleRound;
            patch.maxPlayers = 4;
        }),
    });

    const auto settings = host.room().settings;
    REQUIRE(settings.turnTimer == TurnTimerSeconds::Sixty);
    REQUIRE(settings.matchLength == uno::core::MatchLength::SingleRound);
    REQUIRE(settings.maxPlayers == 4);
}

TEST_CASE("Joining adds the player and tells everyone", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    static_cast<void>(host.received());
    static_cast<void>(guest.received());

    join(guest, code, "Max");

    REQUIRE(acknowledged(guest.received()));
    REQUIRE(host.room().players.size() == 2);
    REQUIRE(guest.room().players.size() == 2);
    const auto second = guest.room().players.at(1);
    REQUIRE(second.nickname == "Max");
    REQUIRE(second.seat == 1);
    REQUIRE_FALSE(second.isHost);
    REQUIRE_FALSE(second.isReady);
}

TEST_CASE("Joining is refused for an unknown code, a full room, a taken nickname or a started match", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host, "Léa");
    host.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) { patch.maxPlayers = 2; })});

    auto first = harness.helloPlayer();
    static_cast<void>(first.received());
    join(first, RoomCode{"ZZZZZZ"}, "Max");
    REQUIRE(refusal(first.received()) == ErrorCode::RoomNotFound);
    join(first, code, "LÉA");
    REQUIRE(refusal(first.received()) == ErrorCode::NicknameTaken);
    join(first, code, "Max");
    REQUIRE(acknowledged(first.received()));

    auto second = harness.helloPlayer();
    static_cast<void>(second.received());
    join(second, code, "Zoé");
    REQUIRE(refusal(second.received()) == ErrorCode::RoomFull);
}

TEST_CASE("Leaving removes the player; the host role passes to the next connected player", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto second = harness.helloPlayer();
    join(second, code, "Max");
    auto third = harness.helloPlayer();
    join(third, code, "Zoé");
    harness.application.onDisconnected(second.connection());
    static_cast<void>(third.received());

    host.send(request::LeaveRoom{});

    const auto room = third.room();
    REQUIRE(room.players.size() == 2);
    const auto& newHost = *std::ranges::find_if(room.players, [](const auto& member) { return member.isHost; });
    REQUIRE(newHost.nickname == "Zoé"); // Max is disconnected
}

TEST_CASE("The room disappears when its last player leaves", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);

    host.send(request::LeaveRoom{});

    REQUIRE(harness.rooms.size() == 0);
    auto late = harness.helloPlayer();
    static_cast<void>(late.received());
    join(late, code, "Max");
    REQUIRE(refusal(late.received()) == ErrorCode::RoomNotFound);
}

TEST_CASE("Leaving without a room is refused", "[app][room]")
{
    AppHarness harness;
    auto player = harness.helloPlayer();
    static_cast<void>(player.received());

    player.send(request::LeaveRoom{});

    REQUIRE(refusal(player.received()) == ErrorCode::NotInRoom);
}

TEST_CASE("Only the host changes settings, and only consistent ones", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");
    static_cast<void>(host.received());
    static_cast<void>(guest.received());

    guest.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) { patch.maxPlayers = 8; })});
    REQUIRE(refusal(guest.received()) == ErrorCode::NotHost);
    host.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) { patch.maxPlayers = 2; })});
    REQUIRE(acknowledged(host.received()));
    host.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) { patch.maxPlayers = 3; })});
    REQUIRE(acknowledged(host.received()));
    auto third = harness.helloPlayer();
    join(third, code, "Zoé");
    static_cast<void>(host.received());
    host.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) { patch.maxPlayers = 2; })});
    REQUIRE(refusal(host.received()) == ErrorCode::InvalidSettings);
    host.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) { patch.sevenZero = true; })});
    REQUIRE(refusal(host.received()) == ErrorCode::InvalidSettings);

    REQUIRE(guest.room().settings.maxPlayers == 3);
}

TEST_CASE("Readiness is shared with the room", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");

    guest.send(request::SetReady{.ready = true});
    REQUIRE(host.room().players.at(1).isReady);
    guest.send(request::SetReady{.ready = false});
    REQUIRE_FALSE(host.room().players.at(1).isReady);
}

TEST_CASE("The host can kick a player, who is told and may join again", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");
    static_cast<void>(guest.received());

    host.send(request::Kick{.playerId = guest.id()});

    const auto closed = ofType<response::RoomClosed>(guest.received());
    REQUIRE(closed.size() == 1);
    REQUIRE(closed.front().reason == response::RoomClosedReason::Kicked);
    REQUIRE(host.room().players.size() == 1);
    join(guest, code, "Max");
    REQUIRE(acknowledged(guest.received()));
}

TEST_CASE("Kicking is for the host, never oneself, and only for members", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");
    const auto stranger = harness.helloPlayer();
    static_cast<void>(host.received());
    static_cast<void>(guest.received());

    guest.send(request::Kick{.playerId = host.id()});
    REQUIRE(refusal(guest.received()) == ErrorCode::NotHost);
    host.send(request::Kick{.playerId = host.id()});
    REQUIRE(refusal(host.received()) == ErrorCode::CannotKickSelf);
    host.send(request::Kick{.playerId = stranger.id()});
    REQUIRE(refusal(host.received()) == ErrorCode::NotInRoom);
}

TEST_CASE("A disconnection is shown to the room and undone by the reconnection", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");

    harness.application.onDisconnected(guest.connection());
    REQUIRE_FALSE(host.room().players.at(1).isConnected);

    auto back = harness.connect();
    back.hello(guest.token());
    REQUIRE(host.room().players.at(1).isConnected);
}

TEST_CASE("Reactions go to the whole room, at most one every two seconds", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");
    static_cast<void>(host.received());
    static_cast<void>(guest.received());

    host.send(request::SendReaction{.emote = request::Emote::Gg});
    REQUIRE(ofType<response::Reaction>(guest.received()).size() == 1);
    host.send(request::SendReaction{.emote = request::Emote::Fire});
    REQUIRE(refusal(host.received()) == ErrorCode::RateLimited);
    harness.clock.advanceMillis(2000);
    host.send(request::SendReaction{.emote = request::Emote::Fire});
    REQUIRE(ofType<response::Reaction>(guest.received()).size() == 1);
}

TEST_CASE("A refused request tells nobody else anything", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto stranger = harness.helloPlayer();
    static_cast<void>(host.received());

    join(stranger, code, "<script>");

    REQUIRE(host.received().empty());
}

TEST_CASE("Bots are not available yet", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    static_cast<void>(createRoom(host));
    static_cast<void>(host.received());

    host.send(request::AddBot{.strategy = request::BotStrategy::Random});

    REQUIRE(refusal(host.received()) == ErrorCode::UnknownType);
}

// Lot H: the lobby shows settings and the kick button to the host only, and the server must not rely on that.
TEST_CASE("A player who is not the host changes nothing: settings and members stay as they were", "[app][room]")
{
    AppHarness harness;
    auto host = harness.helloPlayer();
    const auto code = createRoom(host);
    auto guest = harness.helloPlayer();
    join(guest, code, "Max");
    auto other = harness.helloPlayer();
    join(other, code, "Zoé");
    const auto settingsBefore = host.room().settings;
    const auto membersBefore = host.room().players.size();
    static_cast<void>(host.received());
    static_cast<void>(guest.received());
    static_cast<void>(other.received());

    guest.send(request::UpdateSettings{.settings = patchOf([](RoomSettingsPatch& patch) {
                                           patch.maxPlayers = 10;
                                           patch.drawRule = uno::core::DrawRule::Official;
                                       })});
    guest.send(request::Kick{.playerId = other.id()});

    const auto replies = guest.received();
    REQUIRE(ofType<response::Error>(replies).size() == 2);
    REQUIRE(refusal(replies) == ErrorCode::NotHost);
    REQUIRE(host.received().empty());
    REQUIRE(other.received().empty());
    REQUIRE(host.room().settings == settingsBefore);
    REQUIRE(host.room().players.size() == membersBefore);
}
