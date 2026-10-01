#pragma once

// Wire spelling of every closed set of the protocol (protocol/schema/common.schema.json). One table per
// enum serves both directions, so an encoder and a decoder can never disagree.

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/card.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/turn_order.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace uno::net::detail {

template <typename E>
struct WireName {
    E value;
    std::string_view name;
};

template <typename E>
struct WireNames; // specialised below: `kTable` lists every value of E

template <typename E>
[[nodiscard]] constexpr std::string_view toWire(E value) noexcept
{
    for (const auto& entry : WireNames<E>::kTable) {
        if (entry.value == value) {
            return entry.name;
        }
    }
    return {};
}

template <typename E>
[[nodiscard]] constexpr std::optional<E> fromWire(std::string_view name) noexcept
{
    for (const auto& entry : WireNames<E>::kTable) {
        if (entry.name == name) {
            return entry.value;
        }
    }
    return std::nullopt;
}

template <>
struct WireNames<core::Color> {
    static constexpr std::array kTable{
        WireName{core::Color::Red, "red"},
        WireName{core::Color::Yellow, "yellow"},
        WireName{core::Color::Green, "green"},
        WireName{core::Color::Blue, "blue"},
    };
};

template <>
struct WireNames<core::Rank> {
    static constexpr std::array kTable{
        WireName{core::Rank::Zero, "0"},
        WireName{core::Rank::One, "1"},
        WireName{core::Rank::Two, "2"},
        WireName{core::Rank::Three, "3"},
        WireName{core::Rank::Four, "4"},
        WireName{core::Rank::Five, "5"},
        WireName{core::Rank::Six, "6"},
        WireName{core::Rank::Seven, "7"},
        WireName{core::Rank::Eight, "8"},
        WireName{core::Rank::Nine, "9"},
        WireName{core::Rank::Skip, "skip"},
        WireName{core::Rank::Reverse, "reverse"},
        WireName{core::Rank::DrawTwo, "drawTwo"},
        WireName{core::Rank::Wild, "wild"},
        WireName{core::Rank::WildDrawFour, "wildDrawFour"},
    };
};

template <>
struct WireNames<core::Direction> {
    static constexpr std::array kTable{
        WireName{core::Direction::Clockwise, "clockwise"},
        WireName{core::Direction::CounterClockwise, "counterClockwise"},
    };
};

template <>
struct WireNames<core::MatchLength> {
    static constexpr std::array kTable{
        WireName{core::MatchLength::SingleRound, "singleRound"},
        WireName{core::MatchLength::To250, "to250"},
        WireName{core::MatchLength::To500, "to500"},
    };
};

template <>
struct WireNames<core::PenaltyResponse> {
    static constexpr std::array kTable{
        WireName{core::PenaltyResponse::Accept, "accept"},
        WireName{core::PenaltyResponse::Challenge, "challenge"},
    };
};

template <>
struct WireNames<app::StackingMode> {
    static constexpr std::array kTable{
        WireName{app::StackingMode::Off, "off"},
        WireName{app::StackingMode::SameType, "sameType"},
        WireName{app::StackingMode::Mixed, "mixed"},
    };
};

template <>
struct WireNames<app::WildDrawFourMode> {
    static constexpr std::array kTable{
        WireName{app::WildDrawFourMode::OfficialChallenge, "officialChallenge"},
        WireName{app::WildDrawFourMode::Strict, "strict"},
    };
};

template <>
struct WireNames<app::request::BotStrategy> {
    static constexpr std::array kTable{
        WireName{app::request::BotStrategy::Random, "random"},
        WireName{app::request::BotStrategy::Greedy, "greedy"},
    };
};

template <>
struct WireNames<app::request::Emote> {
    static constexpr std::array kTable{
        WireName{app::request::Emote::Gg, "gg"},       WireName{app::request::Emote::Wow, "wow"},
        WireName{app::request::Emote::Lol, "lol"},     WireName{app::request::Emote::Ouch, "ouch"},
        WireName{app::request::Emote::Think, "think"}, WireName{app::request::Emote::Fire, "fire"},
    };
};

template <>
struct WireNames<app::ErrorCode> {
    static constexpr std::array kTable{
        WireName{app::ErrorCode::MalformedMessage, "MALFORMED_MESSAGE"},
        WireName{app::ErrorCode::UnknownType, "UNKNOWN_TYPE"},
        WireName{app::ErrorCode::UnsupportedVersion, "UNSUPPORTED_VERSION"},
        WireName{app::ErrorCode::MessageTooLarge, "MESSAGE_TOO_LARGE"},
        WireName{app::ErrorCode::RateLimited, "RATE_LIMITED"},
        WireName{app::ErrorCode::SessionRequired, "SESSION_REQUIRED"},
        WireName{app::ErrorCode::SessionExpired, "SESSION_EXPIRED"},
        WireName{app::ErrorCode::NicknameInvalid, "NICKNAME_INVALID"},
        WireName{app::ErrorCode::NicknameTaken, "NICKNAME_TAKEN"},
        WireName{app::ErrorCode::AlreadyInRoom, "ALREADY_IN_ROOM"},
        WireName{app::ErrorCode::NotInRoom, "NOT_IN_ROOM"},
        WireName{app::ErrorCode::RoomNotFound, "ROOM_NOT_FOUND"},
        WireName{app::ErrorCode::RoomFull, "ROOM_FULL"},
        WireName{app::ErrorCode::MatchInProgress, "MATCH_IN_PROGRESS"},
        WireName{app::ErrorCode::NotHost, "NOT_HOST"},
        WireName{app::ErrorCode::CannotKickSelf, "CANNOT_KICK_SELF"},
        WireName{app::ErrorCode::InvalidSettings, "INVALID_SETTINGS"},
        WireName{app::ErrorCode::NotEnoughPlayers, "NOT_ENOUGH_PLAYERS"},
        WireName{app::ErrorCode::PlayersNotReady, "PLAYERS_NOT_READY"},
        WireName{app::ErrorCode::NotYourTurn, "NOT_YOUR_TURN"},
        WireName{app::ErrorCode::InvalidPhase, "INVALID_PHASE"},
        WireName{app::ErrorCode::CardNotInHand, "CARD_NOT_IN_HAND"},
        WireName{app::ErrorCode::IllegalMove, "ILLEGAL_MOVE"},
        WireName{app::ErrorCode::UnoWindowClosed, "UNO_WINDOW_CLOSED"},
    };
};

template <>
struct WireNames<app::IllegalMoveReason> {
    static constexpr std::array kTable{
        WireName{app::IllegalMoveReason::ColorMismatch, "COLOR_MISMATCH"},
        WireName{app::IllegalMoveReason::WildDrawFourIllegal, "WILD_DRAW_FOUR_ILLEGAL"},
        WireName{app::IllegalMoveReason::ColorRequired, "COLOR_REQUIRED"},
        WireName{app::IllegalMoveReason::ColorNotAllowed, "COLOR_NOT_ALLOWED"},
        WireName{app::IllegalMoveReason::SwapTargetRequired, "SWAP_TARGET_REQUIRED"},
        WireName{app::IllegalMoveReason::SwapTargetInvalid, "SWAP_TARGET_INVALID"},
        WireName{app::IllegalMoveReason::OnlyDrawnCardPlayable, "ONLY_DRAWN_CARD_PLAYABLE"},
        WireName{app::IllegalMoveReason::JumpInTooLate, "JUMP_IN_TOO_LATE"},
        WireName{app::IllegalMoveReason::CannotStack, "CANNOT_STACK"},
        WireName{app::IllegalMoveReason::CannotChallenge, "CANNOT_CHALLENGE"},
    };
};

} // namespace uno::net::detail
