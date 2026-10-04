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
    UnoGracePeriod,   // the offender is still the only one who may announce (ADR 0018)
    EffectInProgress, // a turn action came before `actionsOpenAt`: the effect in progress is not over (ADR 0027)
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
    MustPlay,       // guided draw: play a card instead of drawing, or play the drawn one (ADR 0017)
    MustDeclareUno, // house rule: announce UNO before the last card (ADR 0019)
};

} // namespace uno::app
