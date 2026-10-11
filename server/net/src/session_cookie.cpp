#include "uno/net/session_cookie.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace uno::net {
namespace {

constexpr std::size_t kTokenLength = 22;

bool isUrlSafe(char character)
{
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9') || character == '-' || character == '_';
}

std::string_view trimSpaces(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

std::optional<app::SessionToken> sessionTokenFromCookie(std::string_view cookieHeader)
{
    while (!cookieHeader.empty()) {
        const auto separator = cookieHeader.find(';');
        const std::string_view pair = trimSpaces(cookieHeader.substr(0, separator));
        cookieHeader = separator == std::string_view::npos ? std::string_view{} : cookieHeader.substr(separator + 1);
        const auto equals = pair.find('=');
        if (equals == std::string_view::npos || pair.substr(0, equals) != kSessionCookieName) {
            continue;
        }
        const std::string_view value = pair.substr(equals + 1);
        if (value.size() != kTokenLength) {
            return std::nullopt;
        }
        for (const char character : value) {
            if (!isUrlSafe(character)) {
                return std::nullopt;
            }
        }
        return app::SessionToken{std::string(value)};
    }
    return std::nullopt;
}

std::string sessionCookieHeader(const app::SessionToken& token, std::chrono::seconds maxAge)
{
    return std::string(kSessionCookieName) + "=" + token.value + "; Max-Age=" + std::to_string(maxAge.count()) +
           "; Path=/; HttpOnly; Secure; SameSite=Strict";
}

} // namespace uno::net
