#include "uno/app/server_config.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace uno::app {
namespace {

std::string_view trim(std::string_view text)
{
    const auto isSpace = [](char character) { return std::isspace(static_cast<unsigned char>(character)) != 0; };
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

bool isOrigin(std::string_view text)
{
    for (const std::string_view scheme : {"http://", "https://"}) {
        if (text.starts_with(scheme)) {
            const auto authority = text.substr(scheme.size());
            return !authority.empty() && std::ranges::none_of(authority, [](char character) {
                return character == '/' || character == '?' || character == '#' ||
                       std::isspace(static_cast<unsigned char>(character)) != 0;
            });
        }
    }
    return false;
}

} // namespace

std::vector<std::string> defaultAllowedOrigins()
{
    return {"http://localhost:4200", "http://127.0.0.1:4200"};
}

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

std::expected<bool, ConfigError> parseBoolean(std::string_view text)
{
    std::string lowered(text);
    std::ranges::transform(lowered, lowered.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return std::unexpected(ConfigError::BooleanInvalid);
}

std::expected<std::vector<std::string>, ConfigError> parseAllowedOrigins(std::string_view text)
{
    std::vector<std::string> origins;
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto origin = trim(text.substr(0, comma));
        if (!isOrigin(origin)) {
            return std::unexpected(ConfigError::OriginInvalid);
        }
        origins.emplace_back(origin);
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
    }
    if (origins.empty()) {
        return std::unexpected(ConfigError::OriginInvalid);
    }
    return origins;
}

} // namespace uno::app
