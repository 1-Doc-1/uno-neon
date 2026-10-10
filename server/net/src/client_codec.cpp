#include "uno/net/codec.hpp"
#include "uno/net/protocol_version.hpp"

#include "field_parsers.hpp"
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
using detail::parseCardId;
using detail::parseClientVersion;
using detail::Parsed;
using detail::parseMaxPlayers;
using detail::parseMessageId;
using detail::parseNickname;
using detail::parsePlayerId;
using detail::parseRoomCode;
using detail::parseSessionToken;
using detail::parseTurnTimer;
using namespace uno::app;

Parsed<RoomSettingsPatch> parseSettingsPatch(const Json& value)
{
    ObjectReader reader(value, "settings");
    RoomSettingsPatch patch;
    patch.stacking = reader.optional<core::PenaltyStacking>("stacking", detail::parseEnum<core::PenaltyStacking>);
    patch.jumpIn = reader.optional<bool>("jumpIn", detail::parseBool);
    patch.sevenZero = reader.optional<bool>("sevenZero", detail::parseBool);
    patch.drawAmount = reader.optional<core::DrawAmount>("drawAmount", detail::parseEnum<core::DrawAmount>);
    patch.wildDrawFourMode = reader.optional<WildDrawFourMode>("wildDrawFourMode", detail::parseEnum<WildDrawFourMode>);
    patch.turnTimer = reader.optional<TurnTimerSeconds>("turnTimerSeconds", parseTurnTimer);
    patch.matchLength = reader.optional<core::MatchLength>("matchLength", detail::parseEnum<core::MatchLength>);
    patch.maxPlayers = reader.optional<std::uint8_t>("maxPlayers", parseMaxPlayers);
    patch.drawRule = reader.optional<core::DrawRule>("drawRule", detail::parseEnum<core::DrawRule>);
    patch.declareUnoToWin = reader.optional<bool>("declareUnoToWin", detail::parseBool);
    patch.drawTwoMultiplier = reader.optional<core::CardMultiplier>("drawTwoMultiplier", detail::parseCardMultiplier);
    patch.wildDrawFourMultiplier =
        reader.optional<core::CardMultiplier>("wildDrawFourMultiplier", detail::parseCardMultiplier);
    patch.wildDrawFiveMultiplier =
        reader.optional<core::CardMultiplier>("wildDrawFiveMultiplier", detail::parseCardMultiplier);
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

Parsed<request::Body> decodeCreateBotGame(const Json& payload)
{
    ObjectReader reader(payload, "payload");
    request::CreateBotGame game;
    game.nickname = reader.required<std::string>("nickname", parseNickname);
    game.botCount = reader.required<std::uint8_t>("botCount", detail::parseBotCount);
    game.level = reader.required<BotLevel>("level", detail::parseEnum<BotLevel>);
    game.settings = reader.optional<RoomSettingsPatch>("settings", parseSettingsPatch);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return game;
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
    bot.level = reader.required<BotLevel>("level", detail::parseEnum<BotLevel>);
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
    play.targetId = reader.optional<core::PlayerId>("targetId", parsePlayerId);
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
    MessageType{.name = "room.createBotGame", .decode = decodeCreateBotGame},
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

const MessageType* findMessageType(std::string_view name)
{
    for (const MessageType& type : kMessageTypes) {
        if (type.name == name) {
            return &type;
        }
    }
    return nullptr;
}

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
    if (patch.drawAmount) {
        json.emplace("drawAmount", detail::toWire(*patch.drawAmount));
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
    if (patch.drawRule) {
        json.emplace("drawRule", detail::toWire(*patch.drawRule));
    }
    if (patch.declareUnoToWin) {
        json.emplace("declareUnoToWin", *patch.declareUnoToWin);
    }
    if (patch.maxPlayers) {
        json.emplace("maxPlayers", *patch.maxPlayers);
    }
    if (patch.drawTwoMultiplier) {
        json.emplace("drawTwoMultiplier", static_cast<int>(*patch.drawTwoMultiplier));
    }
    if (patch.wildDrawFourMultiplier) {
        json.emplace("wildDrawFourMultiplier", static_cast<int>(*patch.wildDrawFourMultiplier));
    }
    if (patch.wildDrawFiveMultiplier) {
        json.emplace("wildDrawFiveMultiplier", static_cast<int>(*patch.wildDrawFiveMultiplier));
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
    return {.type = "room.addBot", .payload = Json{{"level", detail::toWire(body.level)}}};
}

Description describe(const request::CreateBotGame& body)
{
    Json payload{{"nickname", body.nickname}, {"botCount", body.botCount}, {"level", detail::toWire(body.level)}};
    if (body.settings) {
        payload.emplace("settings", encodeSettingsPatch(*body.settings));
    }
    return {.type = "room.createBotGame", .payload = std::move(payload)};
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
    if (body.targetId) {
        payload.emplace("targetId", body.targetId->value);
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
    const MessageType* const known = findMessageType(typeName);
    if (known == nullptr) {
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
