#pragma once

#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/piles.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/turn_order.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace uno::core {

inline constexpr std::size_t kHandSize = 7;

using Hand = std::vector<Card>;

struct RoundSetup {
    std::vector<PlayerId> seats; // clockwise
    PlayerId dealer;
    std::vector<Card> deck; // already shuffled, in draw order: front() is drawn first
};

// Aggregate of one round (SPEC §7.1). Invariants, established by start() and kept by every member:
// 2 to 10 distinct seated players, one of them current; hands, draw pile and discard pile together
// hold exactly the cards of the deck, each id once; the discard pile is never empty.
// A plain value: copyable without shared state, so a round can be replayed and compared (ADR 0010).
class Round {
public:
    // Deals 7 cards to each player, one at a time starting left of the dealer, then flips the
    // next card onto the discard pile. `random` is only used if a Wild Draw Four is flipped.
    [[nodiscard]] static std::expected<Round, DomainError> start(RoundSetup setup, RandomSource& random);

    [[nodiscard]] std::span<const PlayerId> seats() const noexcept { return turnOrder_.seats(); }
    [[nodiscard]] const PlayerId& dealer() const noexcept { return dealer_; }
    [[nodiscard]] const PlayerId& currentPlayer() const { return turnOrder_.current(); }
    [[nodiscard]] Direction direction() const noexcept { return turnOrder_.direction(); }
    // The span stays valid until the next modification of this Round.
    [[nodiscard]] std::expected<std::span<const Card>, DomainError> hand(const PlayerId& player) const;
    [[nodiscard]] const DrawPile& drawPile() const noexcept { return drawPile_; }
    [[nodiscard]] const DiscardPile& discardPile() const noexcept { return discardPile_; }
    // Empty while a flipped Wild waits for the first player's color choice.
    [[nodiscard]] std::optional<Color> currentColor() const noexcept { return currentColor_; }

    [[nodiscard]] bool operator==(const Round&) const = default;

private:
    Round(TurnOrder turnOrder, PlayerId dealer, std::vector<Hand> hands, DrawPile drawPile, DiscardPile discardPile);

    TurnOrder turnOrder_;
    PlayerId dealer_;
    std::vector<Hand> hands_; // indexed by seat
    DrawPile drawPile_;
    DiscardPile discardPile_;
    std::optional<Color> currentColor_;
};

} // namespace uno::core
