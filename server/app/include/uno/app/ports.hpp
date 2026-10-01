#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/server_message.hpp"

#include <cstdint>
#include <string_view>

// The edges of the application layer (hexagonal architecture): the network adapter of uno_net
// implements the outbound ports and drives the inbound one; tests substitute doubles for them.
namespace uno::app {

// Inbound port: what the transport tells the application about connections. Every call comes from the
// single event-loop thread (SPEC §9.2), so implementations need no lock.
class ConnectionHandler {
public:
    virtual ~ConnectionHandler() = default;

    virtual void onConnected(ConnectionId connection) = 0;
    // Only well-formed requests get here: framing, size and syntax were already checked.
    virtual void onRequest(ConnectionId connection, request::Envelope request) = 0;
    virtual void onDisconnected(ConnectionId connection) = 0;

protected:
    ConnectionHandler() = default;
    ConnectionHandler(const ConnectionHandler&) = default;
    ConnectionHandler(ConnectionHandler&&) = default;
    ConnectionHandler& operator=(const ConnectionHandler&) = default;
    ConnectionHandler& operator=(ConnectionHandler&&) = default;
};

// Outbound port: how the application talks back. Silently ignores a connection that is already gone.
class MessageSink {
public:
    virtual ~MessageSink() = default;

    virtual void send(ConnectionId connection, const response::Message& message) = 0;
    // Closes the connection with a WebSocket close code and a short reason.
    virtual void close(ConnectionId connection, int code, std::string_view reason) = 0;

protected:
    MessageSink() = default;
    MessageSink(const MessageSink&) = default;
    MessageSink(MessageSink&&) = default;
    MessageSink& operator=(const MessageSink&) = default;
    MessageSink& operator=(MessageSink&&) = default;
};

// Server clock, in milliseconds since the Unix epoch (protocol `EpochMillis`). Injected so that deadlines
// and rate limits can be tested without waiting.
class Clock {
public:
    virtual ~Clock() = default;

    [[nodiscard]] virtual std::int64_t nowMillis() const = 0;

protected:
    Clock() = default;
    Clock(const Clock&) = default;
    Clock(Clock&&) = default;
    Clock& operator=(const Clock&) = default;
    Clock& operator=(Clock&&) = default;
};

} // namespace uno::app
