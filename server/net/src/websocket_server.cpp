#include "uno/net/websocket_server.hpp"

#include "uno/net/codec.hpp"

#include <spdlog/spdlog.h>
#include <uwebsockets/App.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace uno::net {
namespace {

constexpr unsigned kHardPayloadLimit = 2 * kMaxMessageBytes;
constexpr unsigned short kIdleTimeoutSeconds = 60;
constexpr unsigned kMaxBackpressureBytes = 64 * 1024;

constexpr int kCloseUnsupportedData = 1003;
constexpr int kClosePolicyViolation = 1008;
constexpr int kCloseMessageTooBig = 1009;

struct ConnectionData {
    app::ConnectionId id;
    unsigned malformedMessages = 0;
};

using Socket = uWS::WebSocket<false, true, ConnectionData>;

} // namespace

struct WebSocketServer::Impl {
    Impl(WebSocketServerConfig serverConfig, ConnectionHandler& connectionHandler)
        : config(std::move(serverConfig)), handler(&connectionHandler), loop(uWS::Loop::get()),
          startedAt(std::chrono::steady_clock::now())
    {
    }

    void handleOpen(Socket* socket)
    {
        connections.emplace(socket->getUserData()->id, socket);
        handler->onConnected(socket->getUserData()->id);
    }

    void handleMessage(Socket* socket, std::string_view text, uWS::OpCode opCode) const
    {
        ConnectionData& data = *socket->getUserData();
        if (opCode != uWS::OpCode::TEXT) {
            socket->end(kCloseUnsupportedData, "Text frames only");
            return;
        }
        if (text.size() > kMaxMessageBytes) {
            socket->end(kCloseMessageTooBig, "MESSAGE_TOO_LARGE");
            return;
        }
        auto request = decodeClientMessage(text);
        if (!request) {
            socket->send(encodeServerMessage(toErrorResponse(request.error())), uWS::OpCode::TEXT);
            if (++data.malformedMessages >= kMaxMalformedMessages) {
                socket->end(kClosePolicyViolation, "Too many malformed messages");
            }
            return;
        }
        handler->onRequest(data.id, std::move(*request));
    }

    void handleClose(Socket* socket)
    {
        const app::ConnectionId id = socket->getUserData()->id;
        connections.erase(id);
        handler->onDisconnected(id);
    }

    void handleUpgrade(uWS::HttpResponse<false>* response, uWS::HttpRequest* request, us_socket_context_t* context)
    {
        if (!config.originPolicy.allows(request->getHeader("origin"))) {
            spdlog::warn("WebSocket refused: origin '{}' is not allowed", request->getHeader("origin"));
            response->writeStatus("403 Forbidden")->end("Origin not allowed");
            return;
        }
        response->template upgrade<ConnectionData>(
            ConnectionData{.id = app::ConnectionId{++lastConnectionId}}, request->getHeader("sec-websocket-key"),
            request->getHeader("sec-websocket-protocol"), request->getHeader("sec-websocket-extensions"), context);
    }

    void handleHealth(uWS::HttpResponse<false>* response) const
    {
        const auto uptime =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startedAt);
        const std::string body = R"({"status":"ok","rooms":)" + std::to_string(config.roomCount()) +
                                 ",\"connections\":" + std::to_string(connections.size()) +
                                 ",\"uptimeSeconds\":" + std::to_string(uptime.count()) + "}";
        response->writeHeader("Content-Type", "application/json")->writeHeader("Cache-Control", "no-store")->end(body);
    }

    WebSocketServerConfig config;
    ConnectionHandler* handler;
    uWS::Loop* loop;
    std::chrono::steady_clock::time_point startedAt;
    std::unique_ptr<uWS::App> app;
    us_listen_socket_t* listenSocket = nullptr;
    std::uint16_t boundPort = 0;
    std::uint64_t lastConnectionId = 0;
    std::unordered_map<app::ConnectionId, Socket*> connections;
};

WebSocketServer::WebSocketServer(WebSocketServerConfig config, ConnectionHandler& handler)
    : impl_(std::make_unique<Impl>(std::move(config), handler))
{
    Impl& impl = *impl_;
    impl.app = std::make_unique<uWS::App>();

    impl.app->get("/health", [&impl](auto* response, auto* /*request*/) { impl.handleHealth(response); });
    impl.app->ws<ConnectionData>(
        "/ws",
        {
            .compression = uWS::DISABLED,
            .maxPayloadLength = kHardPayloadLimit,
            .idleTimeout = kIdleTimeoutSeconds,
            .maxBackpressure = kMaxBackpressureBytes,
            .closeOnBackpressureLimit = true,
            .upgrade = [&impl](auto* response, auto* request,
                               auto* context) { impl.handleUpgrade(response, request, context); },
            .open = [&impl](Socket* socket) { impl.handleOpen(socket); },
            .message = [&impl](Socket* socket, std::string_view text,
                               uWS::OpCode opCode) { impl.handleMessage(socket, text, opCode); },
            .close = [&impl](Socket* socket, int /*code*/, std::string_view /*reason*/) { impl.handleClose(socket); },
        });
    impl.app->any("/*", [](auto* response, auto* /*request*/) { response->writeStatus("404 Not Found")->end(); });

    impl.app->listen(impl.config.port, LIBUS_LISTEN_EXCLUSIVE_PORT,
                     [&impl](us_listen_socket_t* listenSocket) { impl.listenSocket = listenSocket; });
    if (impl.listenSocket == nullptr) {
        throw std::runtime_error("cannot listen on the requested port (already in use?)");
    }
    // uSockets types a listen socket and a socket as unrelated structs: its own API asks for this cast.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* const asSocket = reinterpret_cast<us_socket_t*>(impl.listenSocket);
    impl.boundPort = static_cast<std::uint16_t>(us_socket_local_port(0, asSocket));
}

WebSocketServer::~WebSocketServer() = default;

std::uint16_t WebSocketServer::port() const noexcept
{
    return impl_->boundPort;
}

void WebSocketServer::run()
{
    impl_->app->run();
}

void WebSocketServer::stop()
{
    Impl& impl = *impl_;
    impl.loop->defer([&impl] {
        if (impl.listenSocket != nullptr) {
            us_listen_socket_close(0, impl.listenSocket);
            impl.listenSocket = nullptr;
        }
        impl.app->close();
    });
}

void WebSocketServer::send(app::ConnectionId connection, std::string_view text)
{
    if (const auto found = impl_->connections.find(connection); found != impl_->connections.end()) {
        found->second->send(text, uWS::OpCode::TEXT);
    }
}

void WebSocketServer::close(app::ConnectionId connection, int code, std::string_view reason)
{
    if (const auto found = impl_->connections.find(connection); found != impl_->connections.end()) {
        found->second->end(code, reason);
    }
}

} // namespace uno::net
