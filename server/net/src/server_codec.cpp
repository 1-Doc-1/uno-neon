#include "uno/net/codec.hpp"
#include "uno/net/protocol_version.hpp"

#include "json_reader.hpp"
#include "wire_names.hpp"
#include <nlohmann/json.hpp>

#include <expected>
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

// ---- encoding ----

Json encode(const app::response::Ack& ack)
{
    return Json{{"v", kProtocolVersion}, {"type", "ack"}, {"replyTo", ack.replyTo}};
}

Json encode(const app::response::Error& error)
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

// ---- decoding ----

Parsed<std::string> parseReplyTo(const Json& value)
{
    return detail::parseRestrictedString(value, 1, 32, detail::isUrlSafeCharacter);
}

Parsed<app::response::Message> decodeAck(const Json& envelope)
{
    ObjectReader reader(envelope, "message");
    static_cast<void>(reader.requiredRaw("v"));
    static_cast<void>(reader.requiredRaw("type"));
    auto replyTo = reader.required<std::string>("replyTo", parseReplyTo);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return app::response::Ack{std::move(replyTo)};
}

Parsed<app::IllegalMoveReason> parseDetails(const Json& value)
{
    ObjectReader reader(value, "details");
    const auto reason = reader.required<app::IllegalMoveReason>("reason", detail::parseEnum<app::IllegalMoveReason>);
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return reason;
}

Parsed<app::response::Message> decodeError(const Json& envelope)
{
    ObjectReader reader(envelope, "message");
    static_cast<void>(reader.requiredRaw("v"));
    static_cast<void>(reader.requiredRaw("type"));
    app::response::Error error;
    error.replyTo = reader.optional<std::string>("replyTo", parseReplyTo);
    if (const Json* const payload = reader.requiredRaw("payload")) {
        ObjectReader payloadReader(*payload, "payload");
        error.code = payloadReader.required<app::ErrorCode>("code", detail::parseEnum<app::ErrorCode>);
        error.message = payloadReader.required<std::string>("message", detail::parseString);
        error.reason = payloadReader.optional<app::IllegalMoveReason>("details", parseDetails);
        if (auto finished = payloadReader.finish(); !finished) {
            return std::unexpected(finished.error());
        }
    }
    if (auto finished = reader.finish(); !finished) {
        return std::unexpected(finished.error());
    }
    return error;
}

} // namespace

std::string encodeServerMessage(const app::response::Message& message)
{
    const Json json = std::visit([](const auto& alternative) { return encode(alternative); }, message);
    // `replace` keeps dump() from throwing on invalid UTF-8: a reply must never fail to be written.
    return json.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::expected<app::response::Message, std::string> decodeServerMessage(std::string_view text)
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
    if (typeName == "ack") {
        return decodeAck(envelope);
    }
    if (typeName == "error") {
        return decodeError(envelope);
    }
    return std::unexpected("unknown server message type");
}

std::string_view errorCodeName(app::ErrorCode code) noexcept
{
    return detail::toWire(code);
}

app::response::Error toErrorResponse(const DecodeFailure& failure)
{
    return app::response::Error{
        .replyTo = failure.replyTo,
        .code = failure.code,
        .message = failure.message,
        .reason = std::nullopt,
    };
}

} // namespace uno::net
