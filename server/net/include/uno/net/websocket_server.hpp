#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/net/origin_policy.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace uno::net {

// Largest accepted text frame (SPEC §9.4). uWebSockets itself cuts the connection at twice this size, so
// a hostile client never makes the server buffer more; between the two limits the client is told why.
inline constexpr std::size_t kMaxMessageBytes = 4096;
// Malformed messages tolerated on one connection before it is closed (WebSocket close code 1008).
inline constexpr unsigned kMaxMalformedMessages = 10;

// What the server tells the application layer about a connection.
class ConnectionHandler {
public:
    virtual ~ConnectionHandler() = default;

    virtual void onConnected(app::ConnectionId connection) = 0;
    // Only well-formed requests get here: framing, size and syntax were already checked.
    virtual void onRequest(app::ConnectionId connection, app::request::Envelope request) = 0;
    virtual void onDisconnected(app::ConnectionId connection) = 0;

protected:
    ConnectionHandler() = default;
    ConnectionHandler(const ConnectionHandler&) = default;
    ConnectionHandler(ConnectionHandler&&) = default;
    ConnectionHandler& operator=(const ConnectionHandler&) = default;
    ConnectionHandler& operator=(ConnectionHandler&&) = default;
};

struct WebSocketServerConfig {
    std::uint16_t port{}; // 0 lets the system pick a free port (tests)
    OriginPolicy originPolicy{{}};
    // Number of rooms, for GET /health. Called on the server thread.
    std::function<std::size_t()> roomCount = [] { return std::size_t{0}; };
};

// The single-threaded event loop of the server (SPEC §9.2): one uWebSockets loop carries every
// connection, so the application layer never needs a lock. Serves `GET /health` and the WebSocket
// at `/ws`. Everything but stop() must be called from the thread that constructed it.
class WebSocketServer {
public:
    // Binds the port, throws std::runtime_error when it cannot. `handler` must outlive the server.
    WebSocketServer(WebSocketServerConfig config, ConnectionHandler& handler);
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    WebSocketServer(WebSocketServer&&) = delete;
    WebSocketServer& operator=(WebSocketServer&&) = delete;

    [[nodiscard]] std::uint16_t port() const noexcept;

    // Serves until stop() is called.
    void run();
    // Thread-safe: closes the listening socket and every connection, which ends run().
    void stop();

    // Silently ignored when the connection is gone: a reply can race with a disconnection.
    void send(app::ConnectionId connection, std::string_view text);
    void close(app::ConnectionId connection, int code, std::string_view reason);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace uno::net
