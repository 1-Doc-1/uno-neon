#pragma once

#include <cstdint>

namespace uno::core {

// House rule (SPEC §4, ADR 0041): what happens to a Wild Draw Four played while the hand holds a card of the current
// color.
enum class WildDrawFourMode : std::uint8_t {
    // SPEC §3: the card can always be played, even as a bluff, and its target may accept or challenge it.
    OfficialChallenge,
    // The server refuses a Wild Draw Four that is not legal (the hand holds a card of the current color), so there is
    // nothing to challenge: a legal one is simply drawn.
    Strict,
};

} // namespace uno::core
