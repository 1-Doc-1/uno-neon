#pragma once

#include <cstdint>

namespace uno::core {

// Business errors returned by the engine through std::expected (never thrown).
enum class DomainError : std::uint8_t {
    NotEnoughPlayers,
    TooManyPlayers,
    DuplicatePlayer,
    DealerNotSeated,
    UnknownPlayer,
    DeckTooSmall,
    DuplicateCard,
    NoValidStartingCard,
};

} // namespace uno::core
