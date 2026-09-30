#pragma once

#include "uno/core/card.hpp"

#include <optional>

namespace uno::core {

// SPEC §3: a card may be played if it shares the current color, shares the top card's rank, or is
// a Wild. Wild Draw Four's extra legality condition (no other playable card of the current color)
// and its official challenge are added in step 1.3b. Until then it is judged by this same base
// rule, under which it can never match: its own color is empty, and no Wild Draw Four can yet
// reach the top of the discard pile (the only way there would be through this very function). So
// it is simply rejected for now, like any other non-matching card, with no code of its own.
[[nodiscard]] constexpr bool isPlayable(const Card& candidate, const Card& top,
                                        std::optional<Color> currentColor) noexcept
{
    if (candidate.rank == Rank::Wild) {
        return true;
    }
    if (candidate.color.has_value() && candidate.color == currentColor) {
        return true;
    }
    return candidate.rank == top.rank;
}

} // namespace uno::core
