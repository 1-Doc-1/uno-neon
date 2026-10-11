#include "uno/app/chat.hpp"

#include "uno/app/utf8.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace uno::app {
namespace {

bool isForbidden(CodePoint codePoint)
{
    return codePoint < 0x20 || (codePoint >= 0x7F && codePoint <= 0x9F) // controls, DEL, C1 controls
           || (codePoint >= 0x200B && codePoint <= 0x200F)              // zero-width and direction marks
           || codePoint == 0x2028 || codePoint == 0x2029                 // line and paragraph separators
           || (codePoint >= 0x202A && codePoint <= 0x202E)               // bidirectional embeddings and overrides
           || (codePoint >= 0x2060 && codePoint <= 0x206F)               // invisible operators, deprecated formats
           || (codePoint >= 0xE000 && codePoint <= 0xF8FF)               // private use
           || codePoint == 0xFEFF                                        // byte order mark
           || (codePoint >= 0xFFF0 && codePoint <= 0xFFFF)               // specials
           || (codePoint >= 0xE0000);                                    // tags and supplementary private use
}

std::string_view trimSpaces(std::string_view text)
{
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

std::optional<std::string> validateChatMessage(std::string_view raw)
{
    const std::string_view trimmed = trimSpaces(raw);
    const auto codePoints = decodeUtf8(trimmed);
    if (!codePoints || codePoints->empty() || codePoints->size() > kMaxChatLength) {
        return std::nullopt;
    }
    for (const CodePoint codePoint : *codePoints) {
        if (isForbidden(codePoint)) {
            return std::nullopt;
        }
    }
    return std::string(trimmed);
}

} // namespace uno::app
