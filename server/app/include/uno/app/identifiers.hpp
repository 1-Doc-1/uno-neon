#pragma once

#include <compare> // IWYU pragma: keep (required by the defaulted operator<=>)
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace uno::app {

// Strong types, like uno_core's PlayerId: a token cannot be passed where a room code is expected.

// Secret that lets a client resume its session. Never logged.
struct SessionToken {
    std::string value;

    auto operator<=>(const SessionToken&) const = default;
};

struct RoomCode {
    std::string value;

    auto operator<=>(const RoomCode&) const = default;
};

// One open WebSocket. A session survives its connections; a connection never survives its socket.
struct ConnectionId {
    std::uint64_t value{};

    auto operator<=>(const ConnectionId&) const = default;
};

} // namespace uno::app

template <>
struct std::hash<uno::app::ConnectionId> {
    [[nodiscard]] std::size_t operator()(const uno::app::ConnectionId& id) const noexcept
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template <>
struct std::hash<uno::app::RoomCode> {
    [[nodiscard]] std::size_t operator()(const uno::app::RoomCode& code) const noexcept
    {
        return std::hash<std::string>{}(code.value);
    }
};

template <>
struct std::hash<uno::app::SessionToken> {
    [[nodiscard]] std::size_t operator()(const uno::app::SessionToken& token) const noexcept
    {
        return std::hash<std::string>{}(token.value);
    }
};
