#pragma once

#include "uno/core/card.hpp"

#include <algorithm>
#include <optional>
#include <span>

namespace uno::core {

// SPEC §3: a card may be played if it shares the current color, shares the top card's rank, or is
// a Wild or Wild Draw Four. A Wild Draw Four may always be attempted, legally or as a bluff: its
// extra legality condition (isWildDrawFourLegal below) only decides the outcome of a challenge,
// never whether the card can be played in the first place.
[[nodiscard]] constexpr bool isPlayable(const Card& candidate, const Card& top,
                                        std::optional<Color> currentColor) noexcept
{
    if (isWild(candidate.rank)) {
        return true;
    }
    if (candidate.color.has_value() && candidate.color == currentColor) {
        return true;
    }
    return candidate.rank == top.rank;
}

// Guided draw (ADR 0017): the cards that change the game are the Draw Two and the Wilds. Skip and Reverse count as
// plain cards.
[[nodiscard]] constexpr bool isSpecialCard(const Card& card) noexcept
{
    return card.rank == Rank::DrawTwo || isWild(card.rank);
}

// SPEC §3: a Wild Draw Four is legal only if the hand (with the played card already removed) holds
// no card of the current color at the moment it is played. Another Joker, or a card of the same
// rank in a different color, does not make the play illegal — only a matching color does. Takes a
// definite `currentColor` (not `optional<Color>`) on purpose: a Joker's empty color must never be
// compared against an empty current color, which `std::optional` would consider equal.
[[nodiscard]] constexpr bool isWildDrawFourLegal(std::span<const Card> handWithoutPlayedCard,
                                                 Color currentColor) noexcept
{
    return std::ranges::none_of(handWithoutPlayedCard,
                                [currentColor](const Card& card) { return card.color == currentColor; });
}

} // namespace uno::core
