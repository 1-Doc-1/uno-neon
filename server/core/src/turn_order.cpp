#include "uno/core/turn_order.hpp"

#include "uno/core/detail/duplicates.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/player_id.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

namespace uno::core {

std::expected<TurnOrder, DomainError> TurnOrder::startingLeftOf(std::vector<PlayerId> seats, const PlayerId& dealer)
{
    if (seats.size() < kMinPlayers) {
        return std::unexpected{DomainError::NotEnoughPlayers};
    }
    if (seats.size() > kMaxPlayers) {
        return std::unexpected{DomainError::TooManyPlayers};
    }
    if (detail::hasDuplicates(seats)) {
        return std::unexpected{DomainError::DuplicatePlayer};
    }
    const auto dealerSeat = std::ranges::find(seats, dealer);
    if (dealerSeat == seats.end()) {
        return std::unexpected{DomainError::DealerNotSeated};
    }
    const auto dealerIndex = static_cast<std::size_t>(dealerSeat - seats.begin());
    const auto firstSeat = (dealerIndex + 1) % seats.size();
    return TurnOrder{std::move(seats), firstSeat};
}

TurnOrder::TurnOrder(std::vector<PlayerId> seats, std::size_t currentSeat) noexcept
    : seats_{std::move(seats)}, currentSeat_{currentSeat}
{
}

std::optional<std::size_t> TurnOrder::seatOf(const PlayerId& player) const
{
    const auto seat = std::ranges::find(seats_, player);
    if (seat == seats_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(seat - seats_.begin());
}

void TurnOrder::reverse() noexcept
{
    direction_ = direction_ == Direction::Clockwise ? Direction::CounterClockwise : Direction::Clockwise;
}

std::size_t TurnOrder::nextSeat() const noexcept
{
    const auto playerCount = seats_.size();
    // Adding (count - 1) instead of subtracting 1 keeps the unsigned arithmetic from wrapping below zero.
    const auto step = direction_ == Direction::Clockwise ? std::size_t{1} : playerCount - 1;
    return (currentSeat_ + step) % playerCount;
}

} // namespace uno::core
