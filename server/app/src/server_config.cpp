#include "uno/app/server_config.hpp"

#include <charconv>
#include <limits>
#include <memory>
#include <system_error>

namespace uno::app {

std::expected<std::uint16_t, ConfigError> parsePort(std::string_view text) noexcept
{
    unsigned long value = 0;
    const char* const begin = std::to_address(text.begin());
    const char* const end = std::to_address(text.end());
    const auto [parsedUpTo, error] = std::from_chars(begin, end, value);

    if (error == std::errc::result_out_of_range) {
        return std::unexpected(ConfigError::PortOutOfRange);
    }
    if (error != std::errc{} || parsedUpTo != end) {
        return std::unexpected(ConfigError::PortNotANumber);
    }
    if (value == 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        return std::unexpected(ConfigError::PortOutOfRange);
    }
    return static_cast<std::uint16_t>(value);
}

} // namespace uno::app
