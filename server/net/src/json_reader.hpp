#pragma once

// Strict reading of untrusted JSON (SPEC §9.4: every network input is hostile). Nothing here throws:
// a problem is described by a short English message that ends up in the `error` reply.

#include "wire_names.hpp"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uno::net::detail {

using Json = nlohmann::json;

// Largest integer every JSON implementation (JavaScript included) reads back exactly.
inline constexpr std::int64_t kMaxJsonInteger = (std::int64_t{1} << 53) - 1;

template <typename T>
using Parsed = std::expected<T, std::string>;

[[nodiscard]] inline std::size_t codePointCount(std::string_view utf8) noexcept
{
    std::size_t count = 0;
    for (const char byte : utf8) {
        // Every byte of a UTF-8 sequence but the continuation bytes (10xxxxxx) starts a code point.
        if ((static_cast<unsigned>(static_cast<unsigned char>(byte)) & 0xC0U) != 0x80U) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] inline Parsed<std::string> parseString(const Json& value)
{
    if (!value.is_string()) {
        return std::unexpected("must be a string");
    }
    return value.get<std::string>();
}

[[nodiscard]] inline Parsed<bool> parseBool(const Json& value)
{
    if (!value.is_boolean()) {
        return std::unexpected("must be a boolean");
    }
    return value.get<bool>();
}

// JSON numbers with a fraction or exponent (1.0, 1e2) are rejected even when integral: the protocol
// only has integers, and accepting two spellings would make equal inputs look different.
[[nodiscard]] inline Parsed<std::int64_t> parseInteger(const Json& value, std::int64_t minimum, std::int64_t maximum)
{
    if (!value.is_number_integer()) {
        return std::unexpected("must be an integer");
    }
    // An unsigned number above INT64_MAX cannot be in any range of the protocol.
    if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(maximum)) {
        return std::unexpected("is out of range");
    }
    const auto number = value.get<std::int64_t>();
    if (number < minimum || number > maximum) {
        return std::unexpected("is out of range");
    }
    return number;
}

[[nodiscard]] constexpr bool isUrlSafeCharacter(char character) noexcept
{
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9') || character == '_' || character == '-';
}

// A string of `minLength` to `maxLength` characters, all satisfying `isAllowed`.
template <typename Predicate>
[[nodiscard]] Parsed<std::string> parseRestrictedString(const Json& value, std::size_t minLength, std::size_t maxLength,
                                                        Predicate isAllowed)
{
    auto text = parseString(value);
    if (!text) {
        return text;
    }
    if (text->size() < minLength || text->size() > maxLength) {
        return std::unexpected("has an invalid length");
    }
    for (const char character : *text) {
        if (!isAllowed(character)) {
            return std::unexpected("contains a forbidden character");
        }
    }
    return text;
}

template <typename E>
[[nodiscard]] Parsed<E> parseEnum(const Json& value)
{
    const auto text = parseString(value);
    if (!text) {
        return std::unexpected(text.error());
    }
    if (const auto parsed = fromWire<E>(*text)) {
        return *parsed;
    }
    return std::unexpected("is not an allowed value");
}

// Reads the fields of one JSON object and rejects any field it was not asked about. The first problem
// found is remembered and reported by finish(), so a decoder reads like a list of fields.
class ObjectReader {
public:
    ObjectReader(const Json& value, std::string where) : where_(std::move(where))
    {
        if (value.is_object()) {
            object_ = &value;
        } else {
            error_ = where_ + " must be an object";
        }
    }

    // A mandatory field. On any problem returns a default-constructed T: the caller keeps going and
    // checks finish() once.
    template <typename T, typename Parser>
    [[nodiscard]] T required(std::string_view key, Parser parse)
    {
        auto parsed = read<T>(key, parse, true);
        return parsed ? std::move(*parsed) : T{};
    }

    template <typename T, typename Parser>
    [[nodiscard]] std::optional<T> optional(std::string_view key, Parser parse)
    {
        return read<T>(key, parse, false);
    }

    // The raw value of a mandatory field, for nested structures read by another reader.
    [[nodiscard]] const Json* requiredRaw(std::string_view key) { return rawField(key, true); }

    [[nodiscard]] const Json* optionalRaw(std::string_view key) { return rawField(key, false); }

    void fail(std::string_view key, std::string_view problem)
    {
        if (error_.empty()) {
            error_ = where_ + "." + std::string(key) + " " + std::string(problem);
        }
    }

    [[nodiscard]] Parsed<void> finish()
    {
        if (error_.empty() && object_ != nullptr) {
            for (const auto& [key, ignored] : object_->items()) {
                if (!std::ranges::contains(known_, key)) {
                    return std::unexpected(where_ + "." + key + " is not allowed");
                }
            }
        }
        if (!error_.empty()) {
            return std::unexpected(error_);
        }
        return {};
    }

private:
    [[nodiscard]] const Json* rawField(std::string_view key, bool mandatory)
    {
        known_.emplace_back(key);
        if (object_ == nullptr) {
            return nullptr;
        }
        const auto found = object_->find(key);
        if (found == object_->end()) {
            if (mandatory) {
                fail(key, "is required");
            }
            return nullptr;
        }
        return &*found;
    }

    template <typename T, typename Parser>
    [[nodiscard]] std::optional<T> read(std::string_view key, Parser& parse, bool mandatory)
    {
        const Json* const raw = rawField(key, mandatory);
        if (raw == nullptr) {
            return std::nullopt;
        }
        auto parsed = parse(*raw);
        if (!parsed) {
            fail(key, parsed.error());
            return std::nullopt;
        }
        return std::move(*parsed);
    }

    std::string where_;
    const Json* object_ = nullptr;
    std::vector<std::string> known_;
    std::string error_;
};

} // namespace uno::net::detail
