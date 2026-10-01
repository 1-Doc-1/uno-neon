#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/server_message.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
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

// Identifies a scheduled timer, to cancel it. A default-constructed handle refers to no timer.
struct TimerHandle {
    std::uint64_t id{};

    [[nodiscard]] bool valid() const noexcept { return id != 0; }
    auto operator<=>(const TimerHandle&) const = default;
};

// Outbound port for time (SPEC §9.2): UwsScheduler in production, ManualScheduler (time moves by hand) in tests.
// Callbacks run on the event-loop thread, like everything else.
class Scheduler {
public:
    virtual ~Scheduler() = default;

    // Runs `callback` once, after `delay`.
    [[nodiscard]] virtual TimerHandle schedule(std::chrono::milliseconds delay, std::function<void()> callback) = 0;
    // Forgets a timer that has not fired yet; harmless for a timer that has, or for an invalid handle.
    virtual void cancel(TimerHandle timer) = 0;

protected:
    Scheduler() = default;
    Scheduler(const Scheduler&) = default;
    Scheduler(Scheduler&&) = default;
    Scheduler& operator=(const Scheduler&) = default;
    Scheduler& operator=(Scheduler&&) = default;
};

} // namespace uno::app
