#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace uno::app {

inline constexpr std::uint16_t kDefaultPort = 9001;

struct ServerConfig {
    std::uint16_t port = kDefaultPort;
};

enum class ConfigError : std::uint8_t {
    PortNotANumber,
    PortOutOfRange,
};

// Parses the value of UNO_PORT: a decimal integer in [1, 65535].
[[nodiscard]] std::expected<std::uint16_t, ConfigError> parsePort(std::string_view text) noexcept;

} // namespace uno::app
