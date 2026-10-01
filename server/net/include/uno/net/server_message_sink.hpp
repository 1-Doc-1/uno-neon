#pragma once

#include "uno/app/ports.hpp"
#include "uno/net/websocket_server.hpp"

#include <string_view>

namespace uno::net {

// The outbound port of the application on top of the WebSocket server: encodes each message to JSON and
// writes it to the connection.
class ServerMessageSink final : public app::MessageSink {
public:
    // The server needs the application, which needs this sink: attach() closes the loop once it exists.
    void attach(WebSocketServer& server) noexcept { server_ = &server; }

    void send(app::ConnectionId connection, const app::response::Message& message) override;
    void close(app::ConnectionId connection, int code, std::string_view reason) override;

private:
    WebSocketServer* server_ = nullptr;
};

} // namespace uno::net
