#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace uno::net {

// Why a text frame could not be turned into a request. `replyTo` is the message id when it could be
// read, so the client can tell which of its requests failed.
struct DecodeFailure {
    app::ErrorCode code{};
    std::optional<std::string> replyTo;
    std::string message;

    bool operator==(const DecodeFailure&) const = default;
};

// Strict decoding of a client message (SPEC §8.1, §9.4): the version is checked first
// (UNSUPPORTED_VERSION), then the type (UNKNOWN_TYPE), then the whole shape, where any missing,
// extra or ill-typed field is MALFORMED_MESSAGE. Never throws.
[[nodiscard]] std::expected<app::request::Envelope, DecodeFailure> decodeClientMessage(std::string_view text);

// Encoding of a client message: the server never sends one, but the integration tests of the server
// and the contract tests of the protocol examples speak as a client.
[[nodiscard]] std::string encodeClientMessage(const app::request::Envelope& envelope);

[[nodiscard]] std::string encodeServerMessage(const app::response::Message& message);

// Strict decoding of a server message, for the clients of the integration tests and the protocol
// contract tests. The error is a description of the first problem found.
[[nodiscard]] std::expected<app::response::Message, std::string> decodeServerMessage(std::string_view text);

// Wire spelling of an error code ("NOT_YOUR_TURN"), as written in `error` replies and in the names of the
// protocol/examples/invalid files.
[[nodiscard]] std::string_view errorCodeName(app::ErrorCode code) noexcept;

// The `error` reply for a frame that could not be decoded.
[[nodiscard]] app::response::Error toErrorResponse(const DecodeFailure& failure);

} // namespace uno::net
