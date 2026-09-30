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
};

} // namespace uno::core
