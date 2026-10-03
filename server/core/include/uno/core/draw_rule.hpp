#pragma once

#include <cstdint>

namespace uno::core {

// How a player may draw (SPEC §4, ADR 0017). The engine says what is allowed; when the one move left is forced,
// Round::forcedAction() says which, and the application decides when to play it.
enum class DrawRule : std::uint8_t {
    // House rule: no pointless draws. A player who can play must play, unless a special card is among the playable
    // ones. A drawn card that is playable and plain is played; a special one may be played or kept.
    Guided,
    // SPEC §3: a player may always draw one card instead of playing, and play it if it fits or keep it.
    Official,
};

} // namespace uno::core
