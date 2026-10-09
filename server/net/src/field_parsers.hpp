#pragma once

// Parsers of the shared definitions of protocol/schema/common.schema.json, used by the decoders of both
// directions. Each one checks exactly what the schema checks, and says why it refuses a value.

#include "uno/app/identifiers.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

#include "json_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace uno::net::detail {

using app::kMaxRoomPlayers;
using app::kMinRoomPlayers;
using app::RoomCode;
using app::SessionToken;
using app::TurnTimerSeconds;

inline constexpr std::int64_t kMaxCardId = std::numeric_limits<std::int32_t>::max();
inline constexpr std::size_t kMaxNicknameLength =
    64; // code points; the real rules are checked by the application layer
inline constexpr std::string_view kRoomCodeAlphabet = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
inline constexpr std::size_t kRoomCodeLength = 6;

// ---- field parsers (one function per shared definition of common.schema.json) ----

inline Parsed<std::string> parseMessageId(const Json& value)
{
    return detail::parseRestrictedString(value, 1, 32, detail::isUrlSafeCharacter);
}

inline Parsed<SessionToken> parseSessionToken(const Json& value)
{
    auto text = detail::parseRestrictedString(value, 22, 22, detail::isUrlSafeCharacter);
    if (!text) {
        return std::unexpected(text.error());
    }
    return SessionToken{std::move(*text)};
}

inline Parsed<core::PlayerId> parsePlayerId(const Json& value)
{
    auto text = detail::parseRestrictedString(value, 8, 32, detail::isUrlSafeCharacter);
    if (!text) {
        return std::unexpected(text.error());
    }
    return core::PlayerId{std::move(*text)};
}

inline Parsed<RoomCode> parseRoomCode(const Json& value)
{
    auto text = detail::parseRestrictedString(value, kRoomCodeLength, kRoomCodeLength,
                                              [](char character) { return kRoomCodeAlphabet.contains(character); });
    if (!text) {
        return std::unexpected(text.error());
    }
    return RoomCode{std::move(*text)};
}

inline Parsed<std::string> parseNickname(const Json& value)
{
    auto text = detail::parseString(value);
    if (text && detail::codePointCount(*text) > kMaxNicknameLength) {
        return std::unexpected("is too long");
    }
    return text;
}

inline Parsed<std::string> parseClientVersion(const Json& value)
{
    return detail::parseRestrictedString(value, 1, 32, [](char character) {
        return character == '.' || character == '+' || (character != '_' && detail::isUrlSafeCharacter(character));
    });
}

inline Parsed<core::CardId> parseCardId(const Json& value)
{
    const auto number = detail::parseInteger(value, 0, kMaxCardId);
    if (!number) {
        return std::unexpected(number.error());
    }
    return core::CardId{static_cast<std::uint32_t>(*number)};
}

inline Parsed<TurnTimerSeconds> parseTurnTimer(const Json& value)
{
    const auto seconds = detail::parseInteger(value, 0, 60);
    if (!seconds) {
        return std::unexpected(seconds.error());
    }
    switch (*seconds) {
    case 0:
        return TurnTimerSeconds::Off;
    case 15:
        return TurnTimerSeconds::Fifteen;
    case 30:
        return TurnTimerSeconds::Thirty;
    case 60:
        return TurnTimerSeconds::Sixty;
    default:
        return std::unexpected("is not an allowed value");
    }
}

// The multiplier of a special card of the deck (ADR 0028): the integers 1, 2, 3 and 5, nothing else.
inline Parsed<core::CardMultiplier> parseCardMultiplier(const Json& value)
{
    const auto times = detail::parseInteger(value, 1, 5);
    if (!times) {
        return std::unexpected(times.error());
    }
    const auto found = std::ranges::find_if(core::kCardMultipliers, [&](core::CardMultiplier candidate) {
        return static_cast<std::int64_t>(candidate) == *times;
    });
    if (found == core::kCardMultipliers.end()) {
        return std::unexpected("is not an allowed value");
    }
    return *found;
}

inline Parsed<std::uint8_t> parseMaxPlayers(const Json& value)
{
    const auto count = detail::parseInteger(value, kMinRoomPlayers, kMaxRoomPlayers);
    if (!count) {
        return std::unexpected(count.error());
    }
    return static_cast<std::uint8_t>(*count);
}

} // namespace uno::net::detail
