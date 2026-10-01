#pragma once

#include "uno/core/card.hpp"

#include <cstdint>
#include <numeric>
#include <span>

namespace uno::core {

// SPEC §3: numbers are worth their face value, Skip/Reverse/Draw Two 20, Wild and Wild Draw Four 50.
[[nodiscard]] constexpr std::uint32_t cardPoints(Rank rank) noexcept
{
    if (rank <= Rank::Nine) {
        return static_cast<std::uint32_t>(rank); // Rank::Zero..Nine are declared in face-value order
    }
    return isWild(rank) ? 50U : 20U;
}

[[nodiscard]] constexpr std::uint32_t handPoints(std::span<const Card> hand) noexcept
{
    return std::accumulate(hand.begin(), hand.end(), 0U,
                           [](std::uint32_t total, const Card& card) { return total + cardPoints(card.rank); });
}

} // namespace uno::core
