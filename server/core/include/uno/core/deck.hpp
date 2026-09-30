#pragma once

#include "uno/core/card.hpp"
#include "uno/core/random_source.hpp"

#include <cstddef>
#include <vector>

namespace uno::core {

inline constexpr std::size_t kStandardDeckSize = 108;

// Factory of the official 108-card deck (ADR 0009). Cards come in a fixed canonical order
// (by color, then wild cards) and are NOT shuffled; their ids 0..107 are randomly permuted so
// that an id reveals nothing about its card.
[[nodiscard]] std::vector<Card> createStandardDeck(RandomSource& random);

} // namespace uno::core
