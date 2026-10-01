#pragma once

#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/piles.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/core/turn_phase.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace uno::core {

inline constexpr std::size_t kHandSize = 7;
inline constexpr std::size_t kUnoPenaltyCards = 2;

using Hand = std::vector<Card>;

struct RoundSetup {
    std::vector<PlayerId> seats; // clockwise
    PlayerId dealer;
    std::vector<Card> deck; // already shuffled, in draw order: front() is drawn first
};

// Round::start returns one of these; defined below the class, since it holds a Round by value.
struct RoundStart;

// Aggregate of one round (SPEC §7.1). Invariants, established by start() and kept by every member:
// 2 to 10 distinct seated players, one of them current; hands, draw pile and discard pile together
// hold exactly the cards of the deck, each id once; the discard pile is never empty.
// A plain value: copyable without shared state, so a round can be replayed and compared (ADR 0010).
class Round {
public:
    // Deals 7 cards to each player, one at a time starting left of the dealer, then flips the next
    // card onto the discard pile and resolves its effect, if any (SPEC §3). `random` is used for a
    // Wild Draw Four reflip and, when the flipped card is a Draw Two, its penalty draw.
    [[nodiscard]] static std::expected<RoundStart, DomainError> start(RoundSetup setup, RandomSource& random);

    // Validates and applies one player's intention (SPEC §7.2). Returns the events it produced, in
    // emission order, or the reason it was rejected. Never throws: illegal input is a DomainError.
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    apply(const PlayerId& actor, const PlayerAction& action, RandomSource& random);

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
    [[nodiscard]] const TurnPhase& phase() const noexcept { return phase_; }
    // Whether the player announced UNO and still holds the hand they announced it for.
    [[nodiscard]] bool hasCalledUno(const PlayerId& player) const;
    // The UNO window (SPEC §3, ADR 0011): open on a player who just left themselves with one card
    // without announcing it, until the next play or draw. While open, CatchUno{player} is accepted.
    [[nodiscard]] const std::optional<PlayerId>& unoWindow() const noexcept { return unoWindow_; }

    [[nodiscard]] bool operator==(const Round&) const = default;

private:
    Round(TurnOrder turnOrder, PlayerId dealer, std::vector<Hand> hands, DrawPile drawPile, DiscardPile discardPile);

    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    applyPlayCard(const PlayerId& actor, const PlayCard& action, RandomSource& random);
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> applyDrawCard(const PlayerId& actor,
                                                                                     RandomSource& random);
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> applyPass(const PlayerId& actor);
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> applyChooseColor(const PlayerId& actor,
                                                                                        const ChooseColor& action);
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    applyRespondPenalty(const PlayerId& actor, const RespondPenalty& action, RandomSource& random);
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> applyCallUno(const PlayerId& actor);
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    applyCatchUno(const PlayerId& actor, const CatchUno& action, RandomSource& random);

    // Adds drawn or penalty cards to a hand. A player who receives cards is no longer in the UNO
    // situation they announced, or could be caught in.
    void giveCards(std::size_t seat, std::span<const Card> cards);
    // Opens the UNO window on `player` if they just left themselves with one unannounced card.
    void openUnoWindowIfNeeded(const PlayerId& player, std::size_t seat);

    // Resolves the turn-order effect of a card actually played during normal play: `turnOrder_`'s
    // current player is the one who played it. Returns the events produced, always ending with
    // TurnChanged.
    [[nodiscard]] std::vector<DomainEvent> resolveEffect(Rank rank, RandomSource& random);
    // Resolves the effect, if any, of the first card flipped onto the discard pile (SPEC §3). A
    // flipped card has no real actor: Skip and Draw Two consume the first player's own turn
    // (instead of the next player's, as a played card would), and Reverse hands the very first
    // turn to the dealer. A flipped Wild is handled separately by start(), before this runs.
    [[nodiscard]] std::vector<DomainEvent> resolveFirstCardEffect(Rank rank, RandomSource& random);

    TurnOrder turnOrder_;
    PlayerId dealer_;
    std::vector<Hand> hands_; // indexed by seat
    DrawPile drawPile_;
    DiscardPile discardPile_;
    std::optional<Color> currentColor_;
    TurnPhase phase_{AwaitingPlay{}};
    std::vector<bool> unoCalled_; // indexed by seat
    std::optional<PlayerId> unoWindow_;
};

// Result of Round::start(): the round itself, plus the events produced while resolving the first
// flipped card's effect (SPEC §8.5). Symmetrical with apply()'s return type, so a consumer (the
// future PlayerView projection, step 1.7) can treat both the same way; the event stream never
// starts with a gap.
struct RoundStart {
    Round round;
    std::vector<DomainEvent> events;
};

} // namespace uno::core
