#pragma once

#include "uno/app/identifiers.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace uno::net {

// The session lives in a cookie the browser keeps (ADR 0036): HttpOnly, so no script can read it, Secure, and
// SameSite=Strict, so no other site can make the browser send it.
inline constexpr std::string_view kSessionCookieName = "uno_session";

// The token a `Cookie` request header carries, when it carries a well-formed one (22 URL-safe characters).
[[nodiscard]] std::optional<app::SessionToken> sessionTokenFromCookie(std::string_view cookieHeader);

// The `Set-Cookie` header value that stores (or renews) the session for `maxAge`.
[[nodiscard]] std::string sessionCookieHeader(const app::SessionToken& token, std::chrono::seconds maxAge);

} // namespace uno::net
