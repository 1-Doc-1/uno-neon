#pragma once

#include <cstdint>

namespace uno::app {

// A solo game or a room takes between one and five bots (SPEC §12.1).
inline constexpr std::uint8_t kMinBotsInSoloGame = 1;
inline constexpr std::uint8_t kMaxBotsInSoloGame = 5;

// How well a bot plays (SPEC §4, ADR 0030).
enum class BotLevel : std::uint8_t {
    Easy,   // a legal move at random
    Normal, // a simple heuristic: keeps its Wilds for the end, hits whoever is closest to winning
};

} // namespace uno::app
