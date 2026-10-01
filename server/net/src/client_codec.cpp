#include "uno/net/codec.hpp"
#include "uno/net/protocol_version.hpp"

#include "json_reader.hpp"
#include "wire_names.hpp"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace uno::net {
namespace {

using detail::Json;
using detail::ObjectReader;
using detail::Parsed;
using namespace uno::app;

constexpr std::int64_t kMaxCardId = std::numeric_limits<std::int32_t>::max();
constexpr std::size_t kMaxNicknameLength = 64; // code points; the real rules are checked by the application layer
constexpr std::string_view kRoomCodeAlphabet = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
constexpr std::size_t kRoomCodeLength = 6;

// ---- field parsers (one function per shared definition of common.schema.json) ----

Parsed<std::string> parseMessageId(const Json& value)
{
    return detail::parseRestrictedString(value, 1, 32, detail::isUrlSafeCharacter);
}

Parsed<SessionToken> parseSessionToken(const Json& value)
{
    auto text = detail::parseRestrictedString(value, 22, 22, detail::isUrlSafeCharacter);
    if (!text) {
        return std::unexpected(text.error());
    }
    return SessionToken{std::move(*text)};
}

Parsed<core::PlayerId> parsePlayerId(const Json& value)
{
    auto text = detail::parseRestrictedString(value, 8, 32, detail::isUrlSafeCharacter);
    if (!text) {
        return std::unexpected(text.error());
    }
    return core::PlayerId{std::move(*text)};
}

Parsed<RoomCode> parseRoomCode(const Json& value)
{
    auto text = detail::parseRestrictedString(value, kRoomCodeLength, kRoomCodeLength,
                                              [](char character) { return kRoomCodeAlphabet.contains(character); });
    if (!text) {
        return std::unexpected(text.error());
    }
    return RoomCode{std::move(*text)};
}

Parsed<std::string> parseNickname(const Json& value)
{
    auto text = detail::parseString(value);
    if (text && detail::codePointCount(*text) > kMaxNicknameLength) {
        return std::unexpected("is too long");
    }
    return text;
}

Parsed<std::string> parseClientVersion(const Json& value)
{
    return detail::parseRestrictedString(value, 1, 32, [](char character) {
        return character == '.' || character == '+' || (character != '_' && detail::isUrlSafeCharacter(character));
    });
}

Parsed<core::CardId> parseCardId(const Json& value)
{
    const auto number = detail::parseInteger(value, 0, kMaxCardId);
    if (!number) {
        return std::unexpected(number.error());
    }
    return core::CardId{static_cast<std::uint32_t>(*number)};
}

Parsed<TurnTimerSeconds> parseTurnTimer(const Json& value)
{
    const auto seconds = detail::parseInteger(value, 0, 60);
    if (!seconds) {
        return std::unexpected(seconds.error());
    }
    switch (*seconds) {
    case 0:
        return TurnTimerSeconds::Off;
    case 15:
        return TurnTimerSeconds::Fifteen;
    case 30:
        return TurnTimerSeconds::Thirty;
    case 60:
        return TurnTimerSeconds::Sixty;
    default:
        return std::unexpected("is not an allowed value");
    }
}

Parsed<std::uint8_t> parseMaxPlayers(const Json& value)
{
    const auto count = detail::parseInteger(value, kMinRoomPlayers, kMaxRoomPlayers);
    if (!count) {
        return std::unexpected(count.error());
    }
    return static_cast<std::uint8_t>(*count);
}

Parsed<RoomSettingsPatch> parseSettingsPatch(const Json& value)
{
    ObjectReader reader(value, "settings");
    RoomSettingsPatch patch;
    patch.stacking = reader.optional<StackingMode>("stacking", detail::parseEnum<StackingMode>);
    patch.jumpIn = reader.optional<bool>("jumpIn", detail::parseBool);
    patch.sevenZero = reader.optional<bool>("sevenZero", detail::parseBool);
    patch.drawUntilPlayable = reader.optional<bool>("drawUntilPlayable", detail::parseBool);
    patch.wildDrawFourMode = reader.optional<WildDrawFourMode>("wildDrawFourMode", detail::parseEnum<WildDrawFourMode>);
    patch.turnTimer = reader.optional<TurnTimerSeconds>("turnTimerSeconds", parseTurnTimer);
    patch.matchLength = reader.optional<core::MatchLength>("matchLength", detail::parseEnum<core::MatchLength>);
    patch.maxPlayers = reader.optional<std::uint8_t>("maxPlayers", parseMaxPlayers);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    if (patch == RoomSettingsPatch{}) {
        return std::unexpected("must contain at least one field");
    }
    return patch;
}

// ---- payloads, one decoder per message type ----

using PayloadDecoder = Parsed<request::Body> (*)(const Json& payload);

template <typename Body>
Parsed<request::Body> emptyPayload(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return Body{};
}

Parsed<request::Body> decodeHello(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::Hello hello;
    hello.sessionToken = reader.optional<SessionToken>("sessionToken", parseSessionToken);
    hello.clientVersion = reader.required<std::string>("clientVersion", parseClientVersion);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return hello;
}

Parsed<request::Body> decodeCreateRoom(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::CreateRoom create;
    create.nickname = reader.required<std::string>("nickname", parseNickname);
    create.settings = reader.optional<RoomSettingsPatch>("settings", parseSettingsPatch);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return create;
}

Parsed<request::Body> decodeJoinRoom(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::JoinRoom join;
    join.code = reader.required<RoomCode>("code", parseRoomCode);
    join.nickname = reader.required<std::string>("nickname", parseNickname);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return join;
}

Parsed<request::Body> decodeUpdateSettings(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::UpdateSettings update;
    update.settings = reader.required<RoomSettingsPatch>("settings", parseSettingsPatch);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return update;
}

Parsed<request::Body> decodeSetReady(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::SetReady ready;
    ready.ready = reader.required<bool>("ready", detail::parseBool);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return ready;
}

Parsed<request::Body> decodeKick(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::Kick kick;
    kick.playerId = reader.required<core::PlayerId>("playerId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return kick;
}

Parsed<request::Body> decodeAddBot(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::AddBot bot;
    bot.strategy = reader.required<request::BotStrategy>("strategy", detail::parseEnum<request::BotStrategy>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return bot;
}

Parsed<request::Body> decodePlayCard(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::PlayCard play;
    play.cardId = reader.required<core::CardId>("cardId", parseCardId);
    play.chosenColor = reader.optional<core::Color>("chosenColor", detail::parseEnum<core::Color>);
    play.swapTargetId = reader.optional<core::PlayerId>("swapTargetId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return play;
}

Parsed<request::Body> decodeChooseColor(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::ChooseColor choice;
    choice.color = reader.required<core::Color>("color", detail::parseEnum<core::Color>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return choice;
}

Parsed<request::Body> decodeRespondPenalty(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::RespondPenalty response;
    response.response = reader.required<core::PenaltyResponse>("response", detail::parseEnum<core::PenaltyResponse>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return response;
}

Parsed<request::Body> decodeCatchUno(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::CatchUno target;
    target.targetId = reader.required<core::PlayerId>("targetId", parsePlayerId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return target;
}

Parsed<request::Body> decodeSendReaction(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::SendReaction reaction;
    reaction.emote = reader.required<request::Emote>("emote", detail::parseEnum<request::Emote>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return reaction;
}

struct MessageType {
    std::string_view name;
    PayloadDecoder decode;
};

constexpr std::array kMessageTypes{
    MessageType{.name = "session.hello", .decode = decodeHello},
    MessageType{.name = "room.create", .decode = decodeCreateRoom},
    MessageType{.name = "room.join", .decode = decodeJoinRoom},
    MessageType{.name = "room.leave", .decode = emptyPayload<request::LeaveRoom>},
    MessageType{.name = "room.updateSettings", .decode = decodeUpdateSettings},
    MessageType{.name = "room.setReady", .decode = decodeSetReady},
    MessageType{.name = "room.kick", .decode = decodeKick},
    MessageType{.name = "room.addBot", .decode = decodeAddBot},
    MessageType{.name = "match.start", .decode = emptyPayload<request::StartMatch>},
    MessageType{.name = "match.rematch", .decode = emptyPayload<request::Rematch>},
    MessageType{.name = "match.readyForNextRound", .decode = emptyPayload<request::ReadyForNextRound>},
    MessageType{.name = "game.playCard", .decode = decodePlayCard},
    MessageType{.name = "game.drawCard", .decode = emptyPayload<request::DrawCard>},
    MessageType{.name = "game.pass", .decode = emptyPayload<request::Pass>},
    MessageType{.name = "game.chooseColor", .decode = decodeChooseColor},
    MessageType{.name = "game.respondPenalty", .decode = decodeRespondPenalty},
    MessageType{.name = "game.callUno", .decode = emptyPayload<request::CallUno>},
    MessageType{.name = "game.catchUno", .decode = decodeCatchUno},
    MessageType{.name = "reaction.send", .decode = decodeSendReaction},
};

// The message id, when the envelope has a valid one: even a rejected message is answered with it.
std::optional<std::string> readableId(const Json& envelope)
{
    const auto found = envelope.find("id");
    if (found == envelope.end()) {
        return std::nullopt;
    }
    auto id = parseMessageId(*found);
    return id ? std::optional<std::string>(std::move(*id)) : std::nullopt;
}

DecodeFailure failure(ErrorCode code, std::optional<std::string> replyTo, std::string message)
{
    return DecodeFailure{.code = code, .replyTo = std::move(replyTo), .message = std::move(message)};
}

// ---- encoding ----

Json encodeSettingsPatch(const RoomSettingsPatch& patch)
{
    Json json = Json::object();
    if (patch.stacking) {
        json.emplace("stacking", detail::toWire(*patch.stacking));
    }
    if (patch.jumpIn) {
        json.emplace("jumpIn", *patch.jumpIn);
    }
    if (patch.sevenZero) {
        json.emplace("sevenZero", *patch.sevenZero);
    }
    if (patch.drawUntilPlayable) {
        json.emplace("drawUntilPlayable", *patch.drawUntilPlayable);
    }
    if (patch.wildDrawFourMode) {
        json.emplace("wildDrawFourMode", detail::toWire(*patch.wildDrawFourMode));
    }
    if (patch.turnTimer) {
        json.emplace("turnTimerSeconds", static_cast<int>(*patch.turnTimer));
    }
    if (patch.matchLength) {
        json.emplace("matchLength", detail::toWire(*patch.matchLength));
    }
    if (patch.maxPlayers) {
        json.emplace("maxPlayers", *patch.maxPlayers);
    }
    return json;
}

struct Description {
    std::string_view type;
    Json payload = Json::object();
};

Description describe(const request::Hello& body)
{
    Json payload{{"clientVersion", body.clientVersion}};
    if (body.sessionToken) {
        payload.emplace("sessionToken", body.sessionToken->value);
    }
    return {.type = "session.hello", .payload = std::move(payload)};
}

Description describe(const request::CreateRoom& body)
{
    Json payload{{"nickname", body.nickname}};
    if (body.settings) {
        payload.emplace("settings", encodeSettingsPatch(*body.settings));
    }
    return {.type = "room.create", .payload = std::move(payload)};
}

Description describe(const request::JoinRoom& body)
{
    return {.type = "room.join", .payload = Json{{"code", body.code.value}, {"nickname", body.nickname}}};
}

Description describe(const request::LeaveRoom& /*body*/)
{
    return {.type = "room.leave"};
}

Description describe(const request::UpdateSettings& body)
{
    return {.type = "room.updateSettings", .payload = Json{{"settings", encodeSettingsPatch(body.settings)}}};
}

Description describe(const request::SetReady& body)
{
    return {.type = "room.setReady", .payload = Json{{"ready", body.ready}}};
}

Description describe(const request::Kick& body)
{
    return {.type = "room.kick", .payload = Json{{"playerId", body.playerId.value}}};
}

Description describe(const request::AddBot& body)
{
    return {.type = "room.addBot", .payload = Json{{"strategy", detail::toWire(body.strategy)}}};
}

Description describe(const request::StartMatch& /*body*/)
{
    return {.type = "match.start"};
}

Description describe(const request::Rematch& /*body*/)
{
    return {.type = "match.rematch"};
}

Description describe(const request::ReadyForNextRound& /*body*/)
{
    return {.type = "match.readyForNextRound"};
}

Description describe(const request::PlayCard& body)
{
    Json payload{{"cardId", body.cardId.value}};
    if (body.chosenColor) {
        payload.emplace("chosenColor", detail::toWire(*body.chosenColor));
    }
    if (body.swapTargetId) {
        payload.emplace("swapTargetId", body.swapTargetId->value);
    }
    return {.type = "game.playCard", .payload = std::move(payload)};
}

Description describe(const request::DrawCard& /*body*/)
{
    return {.type = "game.drawCard"};
}

Description describe(const request::Pass& /*body*/)
{
    return {.type = "game.pass"};
}

Description describe(const request::ChooseColor& body)
{
    return {.type = "game.chooseColor", .payload = Json{{"color", detail::toWire(body.color)}}};
}

Description describe(const request::RespondPenalty& body)
{
    return {.type = "game.respondPenalty", .payload = Json{{"response", detail::toWire(body.response)}}};
}

Description describe(const request::CallUno& /*body*/)
{
    return {.type = "game.callUno"};
}

Description describe(const request::CatchUno& body)
{
    return {.type = "game.catchUno", .payload = Json{{"targetId", body.targetId.value}}};
}

Description describe(const request::SendReaction& body)
{
    return {.type = "reaction.send", .payload = Json{{"emote", detail::toWire(body.emote)}}};
}

} // namespace

std::expected<request::Envelope, DecodeFailure> decodeClientMessage(std::string_view text)
{
    const Json envelope = Json::parse(text, nullptr, false);
    if (envelope.is_discarded()) {
        return std::unexpected(failure(ErrorCode::MalformedMessage, std::nullopt, "Invalid JSON"));
    }
    if (!envelope.is_object()) {
        return std::unexpected(failure(ErrorCode::MalformedMessage, std::nullopt, "A message must be a JSON object"));
    }
    const auto replyTo = readableId(envelope);

    const auto version = envelope.find("v");
    if (version == envelope.end() || !version->is_number_integer()) {
        return std::unexpected(failure(ErrorCode::MalformedMessage, replyTo, "v is required and must be an integer"));
    }
    if (*version != kProtocolVersion) {
        return std::unexpected(failure(ErrorCode::UnsupportedVersion, replyTo, "Only protocol version 1 is supported"));
    }

    const auto type = envelope.find("type");
    if (type == envelope.end() || !type->is_string()) {
        return std::unexpected(failure(ErrorCode::MalformedMessage, replyTo, "type is required and must be a string"));
    }
    const auto typeName = type->get<std::string>();
    const auto known = std::ranges::find(kMessageTypes, std::string_view(typeName), &MessageType::name);
    if (known == kMessageTypes.end()) {
        return std::unexpected(failure(ErrorCode::UnknownType, replyTo, "Unknown message type"));
    }

    ObjectReader reader(envelope, "message");
    static_cast<void>(reader.requiredRaw("v"));
    static_cast<void>(reader.requiredRaw("type"));
    const Json* const payload = reader.requiredRaw("payload");
    auto id = reader.required<std::string>("id", parseMessageId);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(failure(ErrorCode::MalformedMessage, replyTo, std::move(finished.error())));
    }

    auto body = known->decode(*payload);
    if (!body) {
        return std::unexpected(failure(ErrorCode::MalformedMessage, replyTo, std::move(body.error())));
    }
    return request::Envelope{.id = std::move(id), .body = std::move(*body)};
}

std::string encodeClientMessage(const request::Envelope& envelope)
{
    const auto description = std::visit([](const auto& body) { return describe(body); }, envelope.body);
    const Json json{
        {"v", kProtocolVersion},
        {"id", envelope.id},
        {"type", description.type},
        {"payload", description.payload},
    };
    return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

} // namespace uno::net
