#pragma once

#include <cstdint>

namespace uno::app {

// Every error the server can answer (SPEC §8.6, protocol `ErrorCode`). The wire spelling lives in the
// codec of uno_net: the application layer only knows the meaning.
enum class ErrorCode : std::uint8_t {
    MalformedMessage,
    UnknownType,
    UnsupportedVersion,
    MessageTooLarge,
    RateLimited,
    SessionRequired,
    SessionExpired,
    NicknameInvalid,
    NicknameTaken,
    AlreadyInRoom,
    NotInRoom,
    RoomNotFound,
    RoomFull,
    MatchInProgress,
    NotHost,
    CannotKickSelf,
    InvalidSettings,
    NotEnoughPlayers,
    PlayersNotReady,
    NotYourTurn,
    InvalidPhase,
    CardNotInHand,
    IllegalMove,
    UnoWindowClosed,
};

// Detail of an IllegalMove error (protocol `IllegalMoveReason`).
enum class IllegalMoveReason : std::uint8_t {
    ColorMismatch,
    WildDrawFourIllegal,
    ColorRequired,
    ColorNotAllowed,
    SwapTargetRequired,
    SwapTargetInvalid,
    OnlyDrawnCardPlayable,
    JumpInTooLate,
    CannotStack,
    CannotChallenge,
};

} // namespace uno::app
