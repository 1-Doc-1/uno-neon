#include "uno/app/utf8.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace uno::app {

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

} // namespace uno::app
