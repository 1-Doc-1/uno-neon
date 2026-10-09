#pragma once

#include "uno/core/card.hpp"
#include "uno/core/random_source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uno::core {

// How many times the host multiplies the copies of a special card (ADR 0028). A closed set: no other value exists.
enum class CardMultiplier : std::uint8_t { One = 1, Two = 2, Three = 3, Five = 5 };

inline constexpr std::array kCardMultipliers{CardMultiplier::One, CardMultiplier::Two, CardMultiplier::Three,
                                             CardMultiplier::Five};

// The composition of the deck beyond the official one: a multiplier for each special card the host may rarefy or
// multiply. The default is the standard deck.
struct DeckSettings {
    CardMultiplier drawTwo{CardMultiplier::One};      // 8 per deck (2 per colour)
    CardMultiplier wildDrawFour{CardMultiplier::One}; // 4 per deck
    CardMultiplier wildDrawFive{CardMultiplier::One}; // 2 per deck

    [[nodiscard]] bool operator==(const DeckSettings&) const = default;
};

inline constexpr std::size_t kStandardDeckSize = 110; // the official 108 cards plus two Wild Draw Five

// The number of cards of each kind of a deck, as a pure function of its settings (the single place that knows the
// composition). Per colour: one 0, two of each of 1..9, Skip and Reverse, and `2 x drawTwo` Draw Two; then four Wild,
// `4 x wildDrawFour` Wild Draw Four and `2 x wildDrawFive` Wild Draw Five, none of which has a colour.
struct DeckComposition {
    std::size_t perColor{}; // cards of each of the four colours
    std::size_t wild{};     // Wild
    std::size_t wildDrawFour{};
    std::size_t wildDrawFive{};

    [[nodiscard]] constexpr std::size_t total() const noexcept
    {
        return (kColors.size() * perColor) + wild + wildDrawFour + wildDrawFive;
    }

    [[nodiscard]] bool operator==(const DeckComposition&) const = default;
};

[[nodiscard]] DeckComposition compositionOf(const DeckSettings& settings) noexcept;

// Factory of a deck (ADR 0009, 0028). Cards come in a fixed canonical order (by color, then wild cards) and are NOT
// shuffled; their ids 0..size-1 are randomly permuted so that an id reveals nothing about its card.
[[nodiscard]] std::vector<Card> createDeck(const DeckSettings& settings, RandomSource& random);

// The standard deck: `createDeck` with every multiplier at one.
[[nodiscard]] std::vector<Card> createStandardDeck(RandomSource& random);

} // namespace uno::core
