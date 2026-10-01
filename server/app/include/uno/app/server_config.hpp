#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace uno::app {

inline constexpr std::uint16_t kDefaultPort = 9001;

// Origins of the Angular dev server (`npm start`): the only ones allowed until UNO_ALLOWED_ORIGINS says otherwise.
[[nodiscard]] std::vector<std::string> defaultAllowedOrigins();

struct ServerConfig {
    std::uint16_t port = kDefaultPort;
    std::vector<std::string> allowedOrigins = defaultAllowedOrigins();
    bool trustedProxy = false; // UNO_TRUSTED_PROXY: believe X-Forwarded-For for the address of a client
};

enum class ConfigError : std::uint8_t {
    PortNotANumber,
    PortOutOfRange,
    OriginInvalid,
    BooleanInvalid,
};

// Parses the value of UNO_PORT: a decimal integer in [1, 65535].
[[nodiscard]] std::expected<std::uint16_t, ConfigError> parsePort(std::string_view text) noexcept;

// Parses a boolean variable (UNO_TRUSTED_PROXY): 1/true/yes/on or 0/false/no/off, in any case.
[[nodiscard]] std::expected<bool, ConfigError> parseBoolean(std::string_view text);

// Parses the value of UNO_ALLOWED_ORIGINS: comma-separated origins, each `http(s)://host[:port]` without
// a path (that is the shape of an Origin header). At least one is required.
[[nodiscard]] std::expected<std::vector<std::string>, ConfigError> parseAllowedOrigins(std::string_view text);

} // namespace uno::app
