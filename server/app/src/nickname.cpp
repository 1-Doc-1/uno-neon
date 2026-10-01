#include "uno/app/nickname.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uno::app {
namespace {

using CodePoint = char32_t;

// Strict UTF-8 decoding: nullopt on a truncated, overlong or out-of-range sequence, or a surrogate.
std::optional<std::vector<CodePoint>> decodeUtf8(std::string_view text)
{
    std::vector<CodePoint> codePoints;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text.at(index));
        std::size_t length = 1;
        CodePoint value = lead;
        CodePoint minimum = 0;
        if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            value = lead & 0x07U;
            minimum = 0x10000;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            value = lead & 0x0FU;
            minimum = 0x800;
        } else if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            value = lead & 0x1FU;
            minimum = 0x80;
        } else if (lead >= 0x80) {
            return std::nullopt;
        }
        if (index + length > text.size()) {
            return std::nullopt;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(text.at(index + offset));
            if ((continuation & 0xC0U) != 0x80U) {
                return std::nullopt;
            }
            value = (value << 6U) | (continuation & 0x3FU);
        }
        const bool surrogate = value >= 0xD800 && value <= 0xDFFF;
        if (value < minimum || value > 0x10FFFF || surrogate) {
            return std::nullopt;
        }
        codePoints.push_back(value);
        index += length;
    }
    return codePoints;
}

bool isAsciiAlphanumeric(CodePoint codePoint)
{
    return (codePoint >= U'0' && codePoint <= U'9') || (codePoint >= U'A' && codePoint <= U'Z') ||
           (codePoint >= U'a' && codePoint <= U'z');
}

// Without a Unicode database: beyond ASCII, everything is accepted as a letter except the blocks of
// punctuation, symbols, combining marks, format characters, private use and emoji.
bool isLetterOutsideAscii(CodePoint codePoint)
{
    struct Range {
        CodePoint first;
        CodePoint last;
    };
    static constexpr auto kRejected = std::to_array<Range>({
        {.first = 0x0080, .last = 0x00BF},    // C1 controls, no-break space, Latin-1 punctuation and symbols
        {.first = 0x00D7, .last = 0x00D7},    // multiplication sign
        {.first = 0x00F7, .last = 0x00F7},    // division sign
        {.first = 0x02B0, .last = 0x036F},    // modifier letters, combining diacritical marks
        {.first = 0x2000, .last = 0x2BFF},    // punctuation, symbols, arrows, box drawing, dingbats
        {.first = 0x3000, .last = 0x303F},    // CJK punctuation
        {.first = 0xD800, .last = 0xF8FF},    // surrogates, private use
        {.first = 0xFE00, .last = 0xFE0F},    // variation selectors
        {.first = 0xFFF0, .last = 0xFFFF},    // specials
        {.first = 0x1F000, .last = 0x1FAFF},  // emoji and pictographs
        {.first = 0xE0000, .last = 0x10FFFF}, // tags, supplementary private use
    });
    for (const Range& range : kRejected) {
        if (codePoint >= range.first && codePoint <= range.last) {
            return false;
        }
    }
    return codePoint >= 0xC0;
}

bool isAllowed(CodePoint codePoint)
{
    if (codePoint < 0x80) {
        return isAsciiAlphanumeric(codePoint) || codePoint == U' ' || codePoint == U'_' || codePoint == U'-';
    }
    return isLetterOutsideAscii(codePoint);
}

CodePoint simpleLowercase(CodePoint codePoint)
{
    if (codePoint >= U'A' && codePoint <= U'Z') {
        return codePoint + 32;
    }
    if ((codePoint >= 0xC0 && codePoint <= 0xDE && codePoint != 0xD7) || (codePoint >= 0x391 && codePoint <= 0x3A9) ||
        (codePoint >= 0x410 && codePoint <= 0x42F)) {
        return codePoint + 32;
    }
    if (codePoint >= 0x400 && codePoint <= 0x40F) {
        return codePoint + 80;
    }
    // Latin Extended-A: capital and small letters alternate (even, then odd).
    if (codePoint >= 0x100 && codePoint <= 0x17F && codePoint % 2 == 0 && codePoint != 0x138) {
        return codePoint + 1;
    }
    return codePoint;
}

void appendUtf8(std::string& out, CodePoint codePoint)
{
    if (codePoint < 0x80) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else if (codePoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
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

std::optional<std::string> validateNickname(std::string_view raw)
{
    const std::string_view trimmed = trimSpaces(raw);
    const auto codePoints = decodeUtf8(trimmed);
    if (!codePoints || codePoints->size() < kMinNicknameLength || codePoints->size() > kMaxNicknameLength) {
        return std::nullopt;
    }
    for (const CodePoint codePoint : *codePoints) {
        if (!isAllowed(codePoint)) {
            return std::nullopt;
        }
    }
    return std::string(trimmed);
}

std::string nicknameKey(std::string_view nickname)
{
    std::string key;
    for (const CodePoint codePoint : decodeUtf8(nickname).value_or(std::vector<CodePoint>{})) {
        appendUtf8(key, simpleLowercase(codePoint));
    }
    return key;
}

} // namespace uno::app
