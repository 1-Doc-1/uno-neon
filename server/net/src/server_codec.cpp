#include "uno/net/codec.hpp"
#include "uno/net/protocol_version.hpp"

#include "field_parsers.hpp"
#include "game_codec.hpp"
#include "json_reader.hpp"
#include "wire_names.hpp"
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace uno::net {
namespace {

using detail::Json;
using detail::ObjectReader;
using detail::Parsed;
using namespace uno::app;

// ---- encoding ----

Json encode(const response::Ack& ack)
{
    return Json{{"v", kProtocolVersion}, {"type", "ack"}, {"replyTo", ack.replyTo}};
}

Json encode(const response::Error& error)
{
    Json payload{{"code", detail::toWire(error.code)}, {"message", error.message}};
    if (error.reason) {
        payload.emplace("details", Json{{"reason", detail::toWire(*error.reason)}});
    }
    Json json{{"v", kProtocolVersion}, {"type", "error"}, {"payload", std::move(payload)}};
    if (error.replyTo) {
        json.emplace("replyTo", *error.replyTo);
    }
    return json;
}

Json encode(const response::Welcome& welcome)
{
    Json payload{{"sessionToken", welcome.sessionToken.value}, {"playerId", welcome.playerId.value}};
    if (welcome.resumedRoomCode) {
        payload.emplace("resumedRoomCode", welcome.resumedRoomCode->value);
    }
    return Json{{"v", kProtocolVersion}, {"type", "session.welcome"}, {"payload", std::move(payload)}};
}

Json encode(const response::RoomView& room)
{
    Json players = Json::array();
    for (const response::RoomMember& member : room.players) {
        players.push_back(Json{
            {"playerId", member.playerId.value},
            {"nickname", member.nickname},
            {"seat", member.seat},
            {"isHost", member.isHost},
            {"isReady", member.isReady},
            {"isConnected", member.isConnected},
            {"isBot", member.isBot},
        });
    }
    return Json{
        {"code", room.code.value},
        {"phase", detail::toWire(room.phase)},
        {"settings", detail::encodeSettings(room.settings)},
        {"players", std::move(players)},
    };
}

Json encode(const response::RoomUpdate& update)
{
    return Json{
        {"v", kProtocolVersion},
        {"type", "room.update"},
        {"payload", Json{{"roomVersion", update.roomVersion}, {"room", encode(update.room)}}},
    };
}

Json encode(const response::GameUpdate& update)
{
    return Json{{"v", kProtocolVersion}, {"type", "game.update"}, {"payload", detail::encodeGameUpdate(update)}};
}

Json encode(const response::Reaction& reaction)
{
    return Json{
        {"v", kProtocolVersion},
        {"type", "reaction"},
        {"payload", Json{{"playerId", reaction.playerId.value}, {"emote", detail::toWire(reaction.emote)}}},
    };
}

Json encode(const response::RoomClosed& closed)
{
    return Json{
        {"v", kProtocolVersion},
        {"type", "room.closed"},
        {"payload", Json{{"reason", detail::toWire(closed.reason)}}},
    };
}

// ---- decoding ----

Parsed<std::string> parseReplyTo(const Json& value)
{
    return detail::parseRestrictedString(value, 1, 32, detail::isUrlSafeCharacter);
}

// The envelope of every server message: v and type, to which the caller adds its own fields.
ObjectReader envelopeReader(const Json& envelope)
{
    ObjectReader reader(envelope, "message");
    static_cast<void>(reader.requiredRaw("v"));
    static_cast<void>(reader.requiredRaw("type"));
    return reader;
}

Parsed<response::Message> decodeAck(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    auto replyTo = reader.required<std::string>("replyTo", parseReplyTo);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return response::Ack{std::move(replyTo)};
}

Parsed<IllegalMoveReason> parseDetails(const Json& value)
{
    ObjectReader reader(value, "details");
    const auto reason = reader.required<IllegalMoveReason>("reason", detail::parseEnum<IllegalMoveReason>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return reason;
}

Parsed<response::Message> decodeError(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    response::Error error;
    error.replyTo = reader.optional<std::string>("replyTo", parseReplyTo);
    if (const Json* const payload = reader.requiredRaw("payload")) {
        ObjectReader payloadReader(*payload, "payload");
        error.code = payloadReader.required<ErrorCode>("code", detail::parseEnum<ErrorCode>);
        error.message = payloadReader.required<std::string>("message", detail::parseString);
        error.reason = payloadReader.optional<IllegalMoveReason>("details", parseDetails);
        if (auto finished = payloadReader.finish(); !finished) {
            return std::unexpected(finished.error());
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return error;
}

Parsed<response::Message> decodeWelcome(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    response::Welcome welcome;
    if (const Json* const payload = reader.requiredRaw("payload")) {
        ObjectReader payloadReader(*payload, "payload");
        welcome.sessionToken = payloadReader.required<SessionToken>("sessionToken", detail::parseSessionToken);
        welcome.playerId = payloadReader.required<core::PlayerId>("playerId", detail::parsePlayerId);
        welcome.resumedRoomCode = payloadReader.optional<RoomCode>("resumedRoomCode", detail::parseRoomCode);
        if (auto finished = payloadReader.finish(); !finished) {
            return std::unexpected(finished.error());
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return welcome;
}

Parsed<std::uint8_t> parseSeat(const Json& value)
{
    const auto number = detail::parseInteger(value, 0, kMaxRoomPlayers - 1);
    if (!number) {
        return std::unexpected(number.error());
    }
    return static_cast<std::uint8_t>(*number);
}

Parsed<response::RoomMember> parseRoomMember(const Json& value)
{
    ObjectReader reader(value, "player");
    response::RoomMember member;
    member.playerId = reader.required<core::PlayerId>("playerId", detail::parsePlayerId);
    member.nickname = reader.required<std::string>("nickname", detail::parseString);
    member.seat = reader.required<std::uint8_t>("seat", parseSeat);
    member.isHost = reader.required<bool>("isHost", detail::parseBool);
    member.isReady = reader.required<bool>("isReady", detail::parseBool);
    member.isConnected = reader.required<bool>("isConnected", detail::parseBool);
    member.isBot = reader.required<bool>("isBot", detail::parseBool);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return member;
}

Parsed<std::vector<response::RoomMember>> parseRoomMembers(const Json& value)
{
    if (!value.is_array() || value.size() > kMaxRoomPlayers) {
        return std::unexpected("must be an array of at most 10 players");
    }
    std::vector<response::RoomMember> members;
    for (const Json& element : value) {
        auto member = parseRoomMember(element);
        if (!member) {
            return std::unexpected(member.error());
        }
        members.push_back(std::move(*member));
    }
    return members;
}

Parsed<response::RoomView> parseRoomView(const Json& value)
{
    ObjectReader reader(value, "room");
    response::RoomView room;
    room.code = reader.required<RoomCode>("code", detail::parseRoomCode);
    room.phase = reader.required<response::RoomPhase>("phase", detail::parseEnum<response::RoomPhase>);
    room.settings = reader.required<RoomSettings>("settings", detail::parseSettings);
    room.players = reader.required<std::vector<response::RoomMember>>("players", parseRoomMembers);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return room;
}

Parsed<std::int64_t> parseVersion(const Json& value)
{
    return detail::parseInteger(value, 0, detail::kMaxJsonInteger);
}

Parsed<response::Message> decodeRoomUpdate(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    response::RoomUpdate update;
    if (const Json* const payload = reader.requiredRaw("payload")) {
        ObjectReader payloadReader(*payload, "payload");
        update.roomVersion =
            static_cast<std::uint64_t>(payloadReader.required<std::int64_t>("roomVersion", parseVersion));
        update.room = payloadReader.required<response::RoomView>("room", parseRoomView);
        if (auto finished = payloadReader.finish(); !finished) {
            return std::unexpected(finished.error());
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return update;
}

Parsed<response::Message> decodeGameUpdate(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    response::GameUpdate update;
    if (const Json* const payload = reader.requiredRaw("payload")) {
        auto parsed = detail::parseGameUpdate(*payload);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        update = std::move(*parsed);
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return update;
}

Parsed<response::Message> decodeReaction(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    response::Reaction reaction;
    if (const Json* const payload = reader.requiredRaw("payload")) {
        ObjectReader payloadReader(*payload, "payload");
        reaction.playerId = payloadReader.required<core::PlayerId>("playerId", detail::parsePlayerId);
        reaction.emote = payloadReader.required<request::Emote>("emote", detail::parseEnum<request::Emote>);
        if (auto finished = payloadReader.finish(); !finished) {
            return std::unexpected(finished.error());
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return reaction;
}

Parsed<response::Message> decodeRoomClosed(const Json& envelope)
{
    auto reader = envelopeReader(envelope);
    response::RoomClosed closed;
    if (const Json* const payload = reader.requiredRaw("payload")) {
        ObjectReader payloadReader(*payload, "payload");
        closed.reason =
            payloadReader.required<response::RoomClosedReason>("reason", detail::parseEnum<response::RoomClosedReason>);
        if (auto finished = payloadReader.finish(); !finished) {
            return std::unexpected(finished.error());
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return closed;
}

using MessageDecoder = Parsed<response::Message> (*)(const Json& envelope);

struct ServerMessageType {
    std::string_view name;
    MessageDecoder decode;
};

constexpr std::array kServerMessageTypes{
    ServerMessageType{.name = "ack", .decode = decodeAck},
    ServerMessageType{.name = "error", .decode = decodeError},
    ServerMessageType{.name = "session.welcome", .decode = decodeWelcome},
    ServerMessageType{.name = "room.update", .decode = decodeRoomUpdate},
    ServerMessageType{.name = "game.update", .decode = decodeGameUpdate},
    ServerMessageType{.name = "reaction", .decode = decodeReaction},
    ServerMessageType{.name = "room.closed", .decode = decodeRoomClosed},
};

} // namespace

std::string encodeServerMessage(const response::Message& message)
{
    const Json json = std::visit([](const auto& alternative) { return encode(alternative); }, message);
    // `replace` keeps dump() from throwing on invalid UTF-8: a reply must never fail to be written.
    return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::expected<response::Message, std::string> decodeServerMessage(std::string_view text)
{
    const Json envelope = Json::parse(text, nullptr, false);
    if (envelope.is_discarded() || !envelope.is_object()) {
        return std::unexpected("not a JSON object");
    }
    const auto version = envelope.find("v");
    if (version == envelope.end() || *version != kProtocolVersion) {
        return std::unexpected("unsupported protocol version");
    }
    const auto type = envelope.find("type");
    if (type == envelope.end() || !type->is_string()) {
        return std::unexpected("type is required and must be a string");
    }
    const auto typeName = type->get<std::string>();
    for (const ServerMessageType& known : kServerMessageTypes) {
        if (known.name == typeName) {
            return known.decode(envelope);
        }
    }
    return std::unexpected("unknown server message type");
}

std::string_view errorCodeName(ErrorCode code) noexcept
{
    return detail::toWire(code);
}

response::Error toErrorResponse(const DecodeFailure& failure)
{
    return response::Error{
        .replyTo = failure.replyTo,
        .code = failure.code,
        .message = failure.message,
        .reason = std::nullopt,
    };
}

} // namespace uno::net
