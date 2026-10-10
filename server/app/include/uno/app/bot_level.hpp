#pragma once

#include <cstdint>

namespace uno::app {

// How well a bot plays (SPEC §4, ADR 0030).
enum class BotLevel : std::uint8_t {
    Easy,   // a legal move at random
    Normal, // a simple heuristic: keeps its Wilds for the end, hits whoever is closest to winning
};

} // namespace uno::app
