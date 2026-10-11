#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace uno::app {

inline constexpr std::size_t kMaxChatLength = 200; // code points

// A chat message (ADR 0037): 1 to 200 code points after trimming the spaces around it, valid UTF-8, and nothing a
// reader could not see or that could reorder the text around it: no control character (a message is one line), no
// zero-width or bidirectional formatting character, no private use code point. Returns the trimmed text, or nothing.
[[nodiscard]] std::optional<std::string> validateChatMessage(std::string_view raw);

} // namespace uno::app
