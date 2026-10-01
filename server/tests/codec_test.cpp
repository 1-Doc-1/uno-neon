#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/net/codec.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <variant>

using uno::app::ErrorCode;
using uno::net::decodeClientMessage;

namespace {

std::string withPayload(const std::string& type, const std::string& payload)
{
    return R"({"v":1,"id":"c-1","type":")" + type + R"(","payload":)" + payload + "}";
}

ErrorCode rejectionOf(const std::string& text)
{
    const auto decoded = decodeClientMessage(text);
    REQUIRE_FALSE(decoded.has_value());
    return decoded.error().code;
}

} // namespace

TEST_CASE("A valid message keeps its id and its fields", "[net][codec]")
{
    const auto decoded = decodeClientMessage(withPayload("game.playCard", R"({"cardId":42,"chosenColor":"red"})"));

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->id == "c-1");
    const auto* play = std::get_if<uno::app::request::PlayCard>(&decoded->body);
    REQUIRE(play != nullptr);
    REQUIRE(play->cardId.value == 42);
    REQUIRE(play->chosenColor == uno::core::Color::Red);
}

TEST_CASE("The version is checked before the type, and the type before the shape", "[net][codec]")
{
    REQUIRE(rejectionOf(R"({"v":2,"id":"c-1","type":"nope","payload":{}})") == ErrorCode::UnsupportedVersion);
    REQUIRE(rejectionOf(R"({"v":1,"id":"c-1","type":"nope","payload":{}})") == ErrorCode::UnknownType);
    REQUIRE(rejectionOf(R"({"v":1,"id":"c-1","type":"game.drawCard","payload":{"extra":1}})") ==
            ErrorCode::MalformedMessage);
}

TEST_CASE("A rejected message is answered with its id when the id is readable", "[net][codec]")
{
    const auto readable = decodeClientMessage(R"({"v":1,"id":"c-9","type":"game.drawCard"})");
    const auto unreadable = decodeClientMessage(R"({"v":1,"id":"not valid!","type":"game.drawCard","payload":{}})");
    const auto notJson = decodeClientMessage("{ nope");

    REQUIRE(readable.error().replyTo == "c-9");
    REQUIRE_FALSE(unreadable.error().replyTo.has_value());
    REQUIRE_FALSE(notJson.error().replyTo.has_value());
}

TEST_CASE("Numbers must be integers in range", "[net][codec]")
{
    REQUIRE(rejectionOf(withPayload("game.playCard", R"({"cardId":1.5})")) == ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("game.playCard", R"({"cardId":1.0})")) == ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("game.playCard", R"({"cardId":-1})")) == ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("game.playCard", R"({"cardId":2147483648})")) == ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("game.playCard", R"({"cardId":"7"})")) == ErrorCode::MalformedMessage);
    REQUIRE(decodeClientMessage(withPayload("game.playCard", R"({"cardId":2147483647})")).has_value());
}

TEST_CASE("Identifiers and codes must have the exact protocol shape", "[net][codec]")
{
    const std::string nickname = R"("nickname":"Léa")";

    REQUIRE(decodeClientMessage(withPayload("room.join", R"({"code":"K7M4X9",)" + nickname + "}")).has_value());
    REQUIRE(rejectionOf(withPayload("room.join", R"({"code":"k7m4x9",)" + nickname + "}")) ==
            ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("room.join", R"({"code":"K7M4X",)" + nickname + "}")) ==
            ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("room.kick", R"({"playerId":"short"})")) == ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("session.hello", R"({"sessionToken":"tooShort","clientVersion":"1"})")) ==
            ErrorCode::MalformedMessage);
    REQUIRE(rejectionOf(withPayload("session.hello", R"({"clientVersion":"1 0"})")) == ErrorCode::MalformedMessage);
}

TEST_CASE("A nickname is bounded in code points, not bytes", "[net][codec]")
{
    std::string accents;
    for (int index = 0; index < 64; ++index) {
        accents += "é"; // two bytes each
    }
    const std::string tooLong = accents + "é";

    REQUIRE(decodeClientMessage(withPayload("room.create", R"({"nickname":")" + accents + R"("})")).has_value());
    REQUIRE(rejectionOf(withPayload("room.create", R"({"nickname":")" + tooLong + R"("})")) ==
            ErrorCode::MalformedMessage);
}

TEST_CASE("A settings patch with allowed values is accepted", "[net][codec]")
{
    const auto text = withPayload("room.updateSettings", R"({"settings":{"turnTimerSeconds":15,"maxPlayers":10}})");

    REQUIRE(decodeClientMessage(text).has_value());
}

TEST_CASE("A settings patch that is empty, out of range or unknown is rejected", "[net][codec]")
{
    const auto settings = GENERATE(as<std::string>{}, "{}", R"({"turnTimerSeconds":20})", R"({"maxPlayers":11})",
                                   R"({"maxPlayers":1})", R"({"stacking":"everything"})", R"({"admin":true})");

    REQUIRE(rejectionOf(withPayload("room.updateSettings", R"({"settings":)" + settings + "}")) ==
            ErrorCode::MalformedMessage);
}

TEST_CASE("Hostile frames never throw and are always MALFORMED_MESSAGE", "[net][codec]")
{
    for (const std::string text : {
             "",
             "null",
             "[]",
             "42",
             "\"text\"",
             "{}",
             "{\"v\":1}",
             "{\"v\":true}",
             R"({"v":1,"type":7})",
             "\xff\xfe",
             R"({"v":1,"id":1,"type":"game.pass"})",
         }) {
        const auto decoded = decodeClientMessage(text);

        REQUIRE_FALSE(decoded.has_value());
        REQUIRE(decoded.error().code == ErrorCode::MalformedMessage);
    }
}

TEST_CASE("A deeply nested document is rejected without exhausting the stack", "[net][codec]")
{
    const std::string nested = std::string(3000, '[') + std::string(3000, ']');

    REQUIRE(rejectionOf(R"({"v":1,"id":"c-1","type":"game.pass","payload":)" + nested + "}") ==
            ErrorCode::MalformedMessage);
}

TEST_CASE("Error codes round-trip through their wire name", "[net][codec]")
{
    REQUIRE(uno::net::errorCodeName(ErrorCode::NotYourTurn) == "NOT_YOUR_TURN");
    REQUIRE(uno::net::errorCodeName(ErrorCode::MessageTooLarge) == "MESSAGE_TOO_LARGE");
}
