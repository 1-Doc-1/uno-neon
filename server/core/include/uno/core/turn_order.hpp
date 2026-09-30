#pragma once

#include "uno/core/domain_error.hpp"
#include "uno/core/player_id.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace uno::core {

inline constexpr std::size_t kMinPlayers = 2;
inline constexpr std::size_t kMaxPlayers = 10;

// Clockwise = increasing seat order (SPEC §3).
enum class Direction : std::uint8_t { Clockwise, CounterClockwise };

// Who plays and in which direction. Invariants: 2 to 10 seated players with distinct ids, and
// the current player is always one of them.
class TurnOrder {
public:
    // `seats` lists the players clockwise; play starts with the player to the left of the dealer.
    [[nodiscard]] static std::expected<TurnOrder, DomainError> startingLeftOf(std::vector<PlayerId> seats,
                                                                              const PlayerId& dealer);

    [[nodiscard]] std::span<const PlayerId> seats() const noexcept { return seats_; }
    [[nodiscard]] Direction direction() const noexcept { return direction_; }
    [[nodiscard]] const PlayerId& current() const { return seats_.at(currentSeat_); }
    [[nodiscard]] std::size_t currentSeat() const noexcept { return currentSeat_; }
    // The player who would play after the current one, without moving the turn.
    [[nodiscard]] const PlayerId& next() const { return seats_.at(nextSeat()); }
    [[nodiscard]] std::optional<std::size_t> seatOf(const PlayerId& player) const;

    void advance() noexcept { currentSeat_ = nextSeat(); }
    void reverse() noexcept;

    [[nodiscard]] bool operator==(const TurnOrder&) const = default;

private:
    TurnOrder(std::vector<PlayerId> seats, std::size_t currentSeat) noexcept;

    [[nodiscard]] std::size_t nextSeat() const noexcept;

    std::vector<PlayerId> seats_;
    std::size_t currentSeat_;
    Direction direction_{Direction::Clockwise};
};

} // namespace uno::core
