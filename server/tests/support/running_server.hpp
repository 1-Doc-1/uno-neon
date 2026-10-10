#pragma once

// A real WebSocketServer on its own thread, bound to a free port, for the integration tests. The
// server must be built on the thread that runs it (uWebSockets keeps its loop in thread-local
// storage), so this helper owns that thread and hands the port back once the socket is listening.

#include "uno/app/client_message.hpp"
#include "uno/app/server_message.hpp"
#include "uno/net/codec.hpp"
#include "uno/net/websocket_server.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace uno::testing {

class RunningServer {
public:
    // `onServer` runs on the server thread, once the server exists and before any client can connect:
    // the place to give a handler the means to answer.
    RunningServer(net::WebSocketServerConfig config, app::ConnectionHandler& handler,
                  std::function<void(net::WebSocketServer&)> onServer = {})
    {
        std::promise<std::uint16_t> started;
        auto port = started.get_future();
        thread_ =
            std::thread([&started, this, config = std::move(config), &handler, onServer = std::move(onServer)] mutable {
                try {
                    net::WebSocketServer server(std::move(config), handler);
                    server_ = &server;
                    if (onServer) {
                        onServer(server);
                    }
                    started.set_value(server.port());
                    server.run();
                } catch (...) {
                    started.set_exception(std::current_exception());
                }
            });
        try {
            port_ = port.get(); // rethrows a failure to start
        } catch (...) {
            thread_.join(); // the thread has already ended: a joinable std::thread would terminate the test run
            throw;
        }
    }

    RunningServer(const RunningServer&) = delete;
    RunningServer& operator=(const RunningServer&) = delete;
    RunningServer(RunningServer&&) = delete;
    RunningServer& operator=(RunningServer&&) = delete;

    ~RunningServer()
    {
        if (server_ != nullptr) {
            server_->stop();
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    std::thread thread_;
    net::WebSocketServer* server_ = nullptr;
    std::uint16_t port_ = 0;
};

// Records what the server reports and answers every request with an `ack`, so a test can both observe
// the application side and read replies on the wire. Thread-safe: callbacks run on the server thread.
class RecordingHandler final : public app::ConnectionHandler {
public:
    void attach(net::WebSocketServer& server) { server_ = &server; }

    void onConnected(app::ConnectionId /*connection*/) override
    {
        const std::scoped_lock lock(mutex_);
        ++connected_;
        changed_.notify_all();
    }

    void onRequest(app::ConnectionId connection, app::request::Envelope request) override
    {
        const std::scoped_lock lock(mutex_);
        server_->send(connection, net::encodeServerMessage(app::response::Ack{request.id}));
        requests_.push_back(std::move(request));
        changed_.notify_all();
    }

    void onDisconnected(app::ConnectionId /*connection*/) override
    {
        const std::scoped_lock lock(mutex_);
        ++disconnected_;
        changed_.notify_all();
    }

    [[nodiscard]] std::vector<app::request::Envelope> requests() const
    {
        const std::scoped_lock lock(mutex_);
        return requests_;
    }

    [[nodiscard]] bool waitForDisconnections(unsigned count,
                                             std::chrono::milliseconds timeout = std::chrono::seconds(2)) const
    {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, timeout, [&] { return disconnected_ >= count; });
    }

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    net::WebSocketServer* server_ = nullptr;
    unsigned connected_ = 0;
    unsigned disconnected_ = 0;
    std::vector<app::request::Envelope> requests_;
};

} // namespace uno::testing
