#pragma once

#include <array>
#include <compare> // IWYU pragma: keep (required by the defaulted operator<=>)
#include <cstdint>
#include <optional>

namespace uno::core {

enum class Color : std::uint8_t { Red, Yellow, Green, Blue };

inline constexpr std::array kColors{Color::Red, Color::Yellow, Color::Green, Color::Blue};

enum class Rank : std::uint8_t {
    Zero,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,
    Skip,
    Reverse,
    DrawTwo,
    Wild,
    WildDrawFour,
};

[[nodiscard]] constexpr bool isWild(Rank rank) noexcept
{
    return rank == Rank::Wild || rank == Rank::WildDrawFour;
}

// Strong type: a CardId cannot be mixed up with an integer or another identifier.
// Assigned randomly for each match, so its value reveals nothing about the card.
struct CardId {
    std::uint32_t value{};

    auto operator<=>(const CardId&) const = default;
};

// Wild cards have no color of their own: `color` is empty for them (ADR 0009).
struct Card {
    CardId id;
    std::optional<Color> color;
    Rank rank{};

    bool operator==(const Card&) const = default;
};

} // namespace uno::core
