#pragma once

// Wire spelling of every closed set of the protocol (protocol/schema/common.schema.json). One table per
// enum serves both directions, so an encoder and a decoder can never disagree.

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/app/server_message.hpp"
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
        WireName{.value = core::Color::Red, .name = "red"},
        WireName{.value = core::Color::Yellow, .name = "yellow"},
        WireName{.value = core::Color::Green, .name = "green"},
        WireName{.value = core::Color::Blue, .name = "blue"},
    };
};

template <>
struct WireNames<core::Rank> {
    static constexpr std::array kTable{
        WireName{.value = core::Rank::Zero, .name = "0"},
        WireName{.value = core::Rank::One, .name = "1"},
        WireName{.value = core::Rank::Two, .name = "2"},
        WireName{.value = core::Rank::Three, .name = "3"},
        WireName{.value = core::Rank::Four, .name = "4"},
        WireName{.value = core::Rank::Five, .name = "5"},
        WireName{.value = core::Rank::Six, .name = "6"},
        WireName{.value = core::Rank::Seven, .name = "7"},
        WireName{.value = core::Rank::Eight, .name = "8"},
        WireName{.value = core::Rank::Nine, .name = "9"},
        WireName{.value = core::Rank::Skip, .name = "skip"},
        WireName{.value = core::Rank::Reverse, .name = "reverse"},
        WireName{.value = core::Rank::DrawTwo, .name = "drawTwo"},
        WireName{.value = core::Rank::Wild, .name = "wild"},
        WireName{.value = core::Rank::WildDrawFour, .name = "wildDrawFour"},
    };
};

template <>
struct WireNames<core::Direction> {
    static constexpr std::array kTable{
        WireName{.value = core::Direction::Clockwise, .name = "clockwise"},
        WireName{.value = core::Direction::CounterClockwise, .name = "counterClockwise"},
    };
};

template <>
struct WireNames<core::MatchLength> {
    static constexpr std::array kTable{
        WireName{.value = core::MatchLength::SingleRound, .name = "singleRound"},
        WireName{.value = core::MatchLength::To250, .name = "to250"},
        WireName{.value = core::MatchLength::To500, .name = "to500"},
    };
};

template <>
struct WireNames<core::PenaltyResponse> {
    static constexpr std::array kTable{
        WireName{.value = core::PenaltyResponse::Accept, .name = "accept"},
        WireName{.value = core::PenaltyResponse::Challenge, .name = "challenge"},
    };
};

template <>
struct WireNames<app::StackingMode> {
    static constexpr std::array kTable{
        WireName{.value = app::StackingMode::Off, .name = "off"},
        WireName{.value = app::StackingMode::SameType, .name = "sameType"},
        WireName{.value = app::StackingMode::Mixed, .name = "mixed"},
    };
};

template <>
struct WireNames<app::WildDrawFourMode> {
    static constexpr std::array kTable{
        WireName{.value = app::WildDrawFourMode::OfficialChallenge, .name = "officialChallenge"},
        WireName{.value = app::WildDrawFourMode::Strict, .name = "strict"},
    };
};

template <>
struct WireNames<app::request::BotStrategy> {
    static constexpr std::array kTable{
        WireName{.value = app::request::BotStrategy::Random, .name = "random"},
        WireName{.value = app::request::BotStrategy::Greedy, .name = "greedy"},
    };
};

template <>
struct WireNames<app::request::Emote> {
    static constexpr std::array kTable{
        WireName{.value = app::request::Emote::Gg, .name = "gg"},
        WireName{.value = app::request::Emote::Wow, .name = "wow"},
        WireName{.value = app::request::Emote::Lol, .name = "lol"},
        WireName{.value = app::request::Emote::Ouch, .name = "ouch"},
        WireName{.value = app::request::Emote::Think, .name = "think"},
        WireName{.value = app::request::Emote::Fire, .name = "fire"},
    };
};

template <>
struct WireNames<app::ErrorCode> {
    static constexpr std::array kTable{
        WireName{.value = app::ErrorCode::MalformedMessage, .name = "MALFORMED_MESSAGE"},
        WireName{.value = app::ErrorCode::UnknownType, .name = "UNKNOWN_TYPE"},
        WireName{.value = app::ErrorCode::UnsupportedVersion, .name = "UNSUPPORTED_VERSION"},
        WireName{.value = app::ErrorCode::MessageTooLarge, .name = "MESSAGE_TOO_LARGE"},
        WireName{.value = app::ErrorCode::RateLimited, .name = "RATE_LIMITED"},
        WireName{.value = app::ErrorCode::SessionRequired, .name = "SESSION_REQUIRED"},
        WireName{.value = app::ErrorCode::SessionExpired, .name = "SESSION_EXPIRED"},
        WireName{.value = app::ErrorCode::NicknameInvalid, .name = "NICKNAME_INVALID"},
        WireName{.value = app::ErrorCode::NicknameTaken, .name = "NICKNAME_TAKEN"},
        WireName{.value = app::ErrorCode::AlreadyInRoom, .name = "ALREADY_IN_ROOM"},
        WireName{.value = app::ErrorCode::NotInRoom, .name = "NOT_IN_ROOM"},
        WireName{.value = app::ErrorCode::RoomNotFound, .name = "ROOM_NOT_FOUND"},
        WireName{.value = app::ErrorCode::RoomFull, .name = "ROOM_FULL"},
        WireName{.value = app::ErrorCode::MatchInProgress, .name = "MATCH_IN_PROGRESS"},
        WireName{.value = app::ErrorCode::NotHost, .name = "NOT_HOST"},
        WireName{.value = app::ErrorCode::CannotKickSelf, .name = "CANNOT_KICK_SELF"},
        WireName{.value = app::ErrorCode::InvalidSettings, .name = "INVALID_SETTINGS"},
        WireName{.value = app::ErrorCode::NotEnoughPlayers, .name = "NOT_ENOUGH_PLAYERS"},
        WireName{.value = app::ErrorCode::PlayersNotReady, .name = "PLAYERS_NOT_READY"},
        WireName{.value = app::ErrorCode::NotYourTurn, .name = "NOT_YOUR_TURN"},
        WireName{.value = app::ErrorCode::InvalidPhase, .name = "INVALID_PHASE"},
        WireName{.value = app::ErrorCode::CardNotInHand, .name = "CARD_NOT_IN_HAND"},
        WireName{.value = app::ErrorCode::IllegalMove, .name = "ILLEGAL_MOVE"},
        WireName{.value = app::ErrorCode::UnoWindowClosed, .name = "UNO_WINDOW_CLOSED"},
    };
};

template <>
struct WireNames<app::IllegalMoveReason> {
    static constexpr std::array kTable{
        WireName{.value = app::IllegalMoveReason::ColorMismatch, .name = "COLOR_MISMATCH"},
        WireName{.value = app::IllegalMoveReason::WildDrawFourIllegal, .name = "WILD_DRAW_FOUR_ILLEGAL"},
        WireName{.value = app::IllegalMoveReason::ColorRequired, .name = "COLOR_REQUIRED"},
        WireName{.value = app::IllegalMoveReason::ColorNotAllowed, .name = "COLOR_NOT_ALLOWED"},
        WireName{.value = app::IllegalMoveReason::SwapTargetRequired, .name = "SWAP_TARGET_REQUIRED"},
        WireName{.value = app::IllegalMoveReason::SwapTargetInvalid, .name = "SWAP_TARGET_INVALID"},
        WireName{.value = app::IllegalMoveReason::OnlyDrawnCardPlayable, .name = "ONLY_DRAWN_CARD_PLAYABLE"},
        WireName{.value = app::IllegalMoveReason::JumpInTooLate, .name = "JUMP_IN_TOO_LATE"},
        WireName{.value = app::IllegalMoveReason::CannotStack, .name = "CANNOT_STACK"},
        WireName{.value = app::IllegalMoveReason::CannotChallenge, .name = "CANNOT_CHALLENGE"},
    };
};

template <>
struct WireNames<app::response::RoomPhase> {
    static constexpr std::array kTable{
        WireName{.value = app::response::RoomPhase::Lobby, .name = "lobby"},
        WireName{.value = app::response::RoomPhase::InGame, .name = "inGame"},
        WireName{.value = app::response::RoomPhase::MatchOver, .name = "matchOver"},
    };
};

template <>
struct WireNames<app::response::RoomClosedReason> {
    static constexpr std::array kTable{
        WireName{.value = app::response::RoomClosedReason::Expired, .name = "expired"},
        WireName{.value = app::response::RoomClosedReason::Kicked, .name = "kicked"},
        WireName{.value = app::response::RoomClosedReason::HostClosed, .name = "hostClosed"},
    };
};

} // namespace uno::net::detail
