#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace uno::app {

using CodePoint = char32_t;

// Strict UTF-8 decoding: nullopt on a truncated, overlong or out-of-range sequence, or a surrogate.
[[nodiscard]] std::optional<std::vector<CodePoint>> decodeUtf8(std::string_view text);

} // namespace uno::app
