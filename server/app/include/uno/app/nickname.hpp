#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace uno::app {

inline constexpr std::size_t kMinNicknameLength = 2;
inline constexpr std::size_t kMaxNicknameLength = 16;

// SPEC §9.3: 2 to 16 characters after trimming; letters of any alphabet, digits, space, `_` and `-`.
// Returns the trimmed nickname, or nothing when it is not acceptable. Nothing that could be markup
// or an invisible character passes (no `<`, quotes, control, format, combining or emoji code points).
[[nodiscard]] std::optional<std::string> validateNickname(std::string_view raw);

// Key under which two nicknames are the same player name: case-insensitive for the common alphabets
// (Latin, Greek, Cyrillic), so "Léa" and "LÉA" collide.
[[nodiscard]] std::string nicknameKey(std::string_view nickname);

} // namespace uno::app
