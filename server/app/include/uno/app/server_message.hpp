#pragma once

#include "uno/app/error_code.hpp"

#include <optional>
#include <string>
#include <variant>

// What the server may send (SPEC §8.4): replies to a request, then messages it pushes.
namespace uno::app::response {

// The request `replyTo` was accepted.
struct Ack {
    std::string replyTo;

    bool operator==(const Ack&) const = default;
};

// The request `replyTo` was rejected. `replyTo` is empty when the message could not be read at all.
struct Error {
    std::optional<std::string> replyTo;
    ErrorCode code{};
    std::string message; // technical English: the client shows its own French text for each code
    std::optional<IllegalMoveReason> reason;

    bool operator==(const Error&) const = default;
};

using Message = std::variant<Ack, Error>;

} // namespace uno::app::response
