#pragma once

#include <cstdint>

namespace uno::core {

// Business errors returned by the engine through std::expected (never thrown).
enum class DomainError : std::uint8_t {
    NotEnoughPlayers,
    TooManyPlayers,
    DuplicatePlayer,
    DealerNotSeated,
    UnknownPlayer,
    DeckTooSmall,
    DuplicateCard,
    NoValidStartingCard,
    // Step 1.3a: PlayerAction validation (SPEC §8.6).
    NotYourTurn,
    InvalidPhase,
    CardNotInHand,
    ColorMismatch,         // ILLEGAL_MOVE / COLOR_MISMATCH
    ColorRequired,         // ILLEGAL_MOVE / COLOR_REQUIRED
    ColorNotAllowed,       // ILLEGAL_MOVE / COLOR_NOT_ALLOWED
    OnlyDrawnCardPlayable, // ILLEGAL_MOVE / ONLY_DRAWN_CARD_PLAYABLE
    // Step 1.4: UNO.
    UnoWindowClosed, // UNO_WINDOW_CLOSED: nobody can be caught right now (or no longer, or not that player)
    CannotCatchSelf,
    // ADR 0017: guided draw.
    MustPlay, // ILLEGAL_MOVE / MUST_PLAY: the player has to play a card instead of drawing, or to play the drawn one
    // ADR 0019: declare UNO to win.
    MustDeclareUno, // ILLEGAL_MOVE / MUST_DECLARE_UNO: the last card cannot be played before UNO is announced
};

} // namespace uno::core
