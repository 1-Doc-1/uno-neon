// Composition root: reads the configuration and wires the layers together.
#include "uno/app/server_config.hpp"
#include "uno/core/build_info.hpp"
#include "uno/net/crypto_runtime.hpp"
#include "uno/net/protocol_version.hpp"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <string>

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
    const char* const value = std::getenv(name);
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
    return config;
}

int run()
{
    if (!uno::net::initializeCryptoRuntime()) {
        spdlog::critical("libsodium initialisation failed");
        return EXIT_FAILURE;
    }

    const auto config = loadConfig();
    if (!config) {
        spdlog::critical("{}", config.error());
        return EXIT_FAILURE;
    }

    spdlog::info("uno_server {} (protocol v{}) configured on port {}", uno::core::projectVersion(),
                 uno::net::kProtocolVersion, config->port);
    spdlog::warn("network layer not implemented yet (phase 2), exiting");
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    // Last line of defence: business errors travel as std::expected, only truly
    // exceptional failures (allocation, logger I/O) can reach this point.
    try {
        return run();
    } catch (const std::exception& error) {
        // Nothing more can be done if writing to stderr fails.
        static_cast<void>(std::fputs(error.what(), stderr));
        return EXIT_FAILURE;
    }
}
