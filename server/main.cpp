// Composition root: reads the configuration and wires the layers together.
#include "uno/app/server_config.hpp"
#include "uno/core/build_info.hpp"
#include "uno/net/codec.hpp"
#include "uno/net/crypto_runtime.hpp"
#include "uno/net/protocol_version.hpp"
#include "uno/net/websocket_server.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {

std::optional<std::string> readEnvironmentVariable(const char* name)
{
#ifdef _MSC_VER
    // MSVC deprecates std::getenv (C4996); _dupenv_s returns an owned copy instead.
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    const std::unique_ptr<char, decltype(&std::free)> owner(buffer, &std::free);
    return std::string(owner.get());
#else
    // Read once at startup, before any other thread exists: no concurrent setenv possible.
    const char* const value = std::getenv(name); // NOLINT(concurrency-mt-unsafe)
    return value == nullptr ? std::nullopt : std::optional<std::string>(value);
#endif
}

std::expected<uno::app::ServerConfig, std::string> loadConfig()
{
    uno::app::ServerConfig config;
    if (const auto port = readEnvironmentVariable("UNO_PORT")) {
        const auto parsedPort = uno::app::parsePort(*port);
        if (!parsedPort) {
            return std::unexpected("UNO_PORT must be an integer between 1 and 65535, got '" + *port + "'");
        }
        config.port = *parsedPort;
    }
    if (const auto origins = readEnvironmentVariable("UNO_ALLOWED_ORIGINS")) {
        auto parsedOrigins = uno::app::parseAllowedOrigins(*origins);
        if (!parsedOrigins) {
            return std::unexpected(
                "UNO_ALLOWED_ORIGINS must list origins like https://uno.example.com, separated by commas, got '" +
                *origins + "'");
        }
        config.allowedOrigins = std::move(*parsedOrigins);
    }
    return config;
}

// Until sessions and rooms exist (step 2.3), every well-formed request is answered with an error.
class SessionlessHandler final : public uno::net::ConnectionHandler {
public:
    void attach(uno::net::WebSocketServer& server) noexcept { server_ = &server; }

    void onConnected(uno::app::ConnectionId connection) override
    {
        spdlog::debug("connection {} opened", connection.value);
    }

    void onRequest(uno::app::ConnectionId connection, uno::app::request::Envelope request) override
    {
        const uno::app::response::Error error{
            .replyTo = std::move(request.id),
            .code = uno::app::ErrorCode::SessionRequired,
            .message = "Sessions are not available yet",
            .reason = std::nullopt,
        };
        server_->send(connection, uno::net::encodeServerMessage(error));
    }

    void onDisconnected(uno::app::ConnectionId connection) override
    {
        spdlog::debug("connection {} closed", connection.value);
    }

private:
    uno::net::WebSocketServer* server_ = nullptr;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): written by a signal handler
std::atomic<bool> gStopRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free, "needed to be set from a signal handler");

extern "C" void requestStop(int /*signal*/)
{
    gStopRequested.store(true);
}

int run()
{
    if (!uno::net::initializeCryptoRuntime()) {
        spdlog::critical("libsodium initialisation failed");
        return EXIT_FAILURE;
    }

    if (const auto level = readEnvironmentVariable("UNO_LOG_LEVEL")) {
        spdlog::set_level(spdlog::level::from_str(*level));
    }

    const auto config = loadConfig();
    if (!config) {
        spdlog::critical("{}", config.error());
        return EXIT_FAILURE;
    }

    SessionlessHandler handler;
    uno::net::WebSocketServer server(
        {.port = config->port, .originPolicy = uno::net::OriginPolicy(config->allowedOrigins)}, handler);
    handler.attach(server);

    static_cast<void>(std::signal(SIGINT, requestStop));
    static_cast<void>(std::signal(SIGTERM, requestStop));
    // A signal handler may only set a flag: this thread turns it into a (thread-safe) stop().
    const std::jthread signalWatcher([&server](const std::stop_token& stop) {
        while (!stop.stop_requested() && !gStopRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (gStopRequested.load()) {
            server.stop();
        }
    });

    spdlog::info("uno_server {} (protocol v{}) listening on port {}", uno::core::projectVersion(),
                 uno::net::kProtocolVersion, server.port());
    server.run();
    spdlog::info("uno_server stopped");
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    // Last line of defence: business errors travel as std::expected, only truly
    // exceptional failures (allocation, logger I/O, port already in use) can reach this point.
    try {
        return run();
    } catch (const std::exception& error) {
        // Nothing more can be done if writing to stderr fails.
        static_cast<void>(std::fputs(error.what(), stderr));
        return EXIT_FAILURE;
    }
}
