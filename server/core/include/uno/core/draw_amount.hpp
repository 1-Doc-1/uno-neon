#pragma once

#include <cstdint>

namespace uno::core {

// How many cards a player who cannot play draws (SPEC §4, ADR 0024). Independent of DrawRule, which decides whether
// drawing is allowed at all. A voluntary draw (the player could have played) is always a single card.
enum class DrawAmount : std::uint8_t {
    // House rule: keep drawing, in one atomic action, until a card is playable or both piles are empty.
    UntilPlayable,
    // SPEC §3: a single card.
    One,
};

} // namespace uno::core
