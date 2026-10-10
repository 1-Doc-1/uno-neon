#pragma once

#include "uno/core/card.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace uno::core {

inline constexpr std::size_t kDrawTwoPenaltyCards = 2;
inline constexpr std::size_t kWildDrawFourPenaltyCards = 4;
inline constexpr std::size_t kWildDrawFivePenaltyCards = 5; // each Wild Draw Five of a chain adds this much

// House rule (SPEC §4, ADR 0029): what a player may do against a pending penalty.
enum class PenaltyStacking : std::uint8_t {
    // SPEC §3: a Draw Two is drawn at once, a Wild Draw Four is accepted or challenged, only a Wild Draw Five answers a
    // Wild Draw Five (ADR 0028).
    Official,
    // Penalty cards pile up, weakest to strongest: +2 < +4 < +5. A penalty card goes on a pending penalty of a lower or
    // equal level, whatever its color, and the amounts add up. A Wild Draw Four cannot be challenged.
    Ladder,
};

// The cards that inflict a penalty, in the order of the ladder; empty for any other card.
[[nodiscard]] constexpr std::optional<int> penaltyLevel(Rank rank) noexcept
{
    switch (rank) {
    case Rank::DrawTwo:
        return 1;
    case Rank::WildDrawFour:
        return 2;
    case Rank::WildDrawFive:
        return 3;
    default:
        return std::nullopt;
    }
}

// How many cards a penalty card makes its target draw (0 for a card that is not one).
[[nodiscard]] constexpr std::size_t penaltyCards(Rank rank) noexcept
{
    switch (rank) {
    case Rank::DrawTwo:
        return kDrawTwoPenaltyCards;
    case Rank::WildDrawFour:
        return kWildDrawFourPenaltyCards;
    case Rank::WildDrawFive:
        return kWildDrawFivePenaltyCards;
    default:
        return 0;
    }
}

// Whether a card of rank `candidate` may be played on a pending penalty last raised by a card of rank `pending`.
[[nodiscard]] constexpr bool canStackOn(Rank candidate, Rank pending) noexcept
{
    const auto level = penaltyLevel(candidate);
    const auto required = penaltyLevel(pending);
    return level.has_value() && required.has_value() && *level >= *required;
}

} // namespace uno::core
