#pragma once

#include "uno/app/identifiers.hpp"
#include "uno/app/ports.hpp"
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

// Limits on what a client may send (SPEC §9.4): a token bucket per connection for every message, and per address
// for the requests that create rooms or try room codes.
struct RateLimits {
    double burst = 20;
    double perSecond = 10;
    std::size_t createsPerMinute = 5;
    std::size_t joinsPerMinute = 20;
};

struct WebSocketServerConfig {
    std::uint16_t port{}; // 0 lets the system pick a free port (tests)
    OriginPolicy originPolicy{{}};
    RateLimits rateLimits;
    // Whether X-Forwarded-For can be believed to tell the real address of a client (UNO_TRUSTED_PROXY).
    bool trustedProxy = false;
    // Number of rooms, for GET /health. Called on the server thread.
    std::function<std::size_t()> roomCount = [] { return std::size_t{0}; };
    // Runs on the loop thread when the server stops, once its sockets are closed: the place to cancel timers.
    std::function<void()> onStop = [] {};
};

// The single-threaded event loop of the server (SPEC §9.2): one uWebSockets loop carries every
// connection, so the application layer never needs a lock. Serves `GET /health` and the WebSocket
// at `/ws`. Everything but stop() must be called from the thread that constructed it.
class WebSocketServer {
public:
    // Binds the port, throws std::runtime_error when it cannot. `handler` must outlive the server.
    WebSocketServer(WebSocketServerConfig config, app::ConnectionHandler& handler);
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
