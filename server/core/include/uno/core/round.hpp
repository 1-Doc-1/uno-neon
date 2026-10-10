#pragma once

#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_amount.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/penalty_stacking.hpp"
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
inline constexpr std::size_t kFailedChallengePenaltyCards = 6; // a challenge lost against a legal +4
inline constexpr std::size_t kUnoPenaltyCards = 2;

using Hand = std::vector<Card>;

// A penalty a played card inflicts: who draws, and how many cards in all (the whole stack, ADR 0029).
struct Penalty {
    PlayerId target;
    std::size_t total{};
};

struct RoundSetup {
    std::vector<PlayerId> seats; // clockwise
    PlayerId dealer;
    std::vector<Card> deck; // already shuffled, in draw order: front() is drawn first
    DrawRule drawRule{DrawRule::Official};
    DrawAmount drawAmount{DrawAmount::One};              // ADR 0024
    bool declareUnoToWin{false};                         // house rule (ADR 0019): the last card needs an announcement
    PenaltyStacking stacking{PenaltyStacking::Official}; // house rule (ADR 0029)
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

    // Takes a player out of the round (they left for good, SPEC §5): their cards go under the draw pile and, if it
    // was their turn or they were the target of a pending penalty, play carries on without them (an unanswered
    // Wild Draw Four is void; a first Wild without a color gets a random one). Returns the events this causes.
    // At least three players must be seated: with two, the match is decided by forfeit (Match::removePlayer).
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> removePlayer(const PlayerId& player,
                                                                                    RandomSource& random);

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
    [[nodiscard]] DrawRule drawRule() const noexcept { return drawRule_; }
    [[nodiscard]] DrawAmount drawAmount() const noexcept { return drawAmount_; }
    [[nodiscard]] bool declareUnoToWin() const noexcept { return declareUnoToWin_; }
    [[nodiscard]] PenaltyStacking stacking() const noexcept { return stacking_; }
    // Whether `player` is stuck on their last card for want of an announcement (ADR 0019): the house rule is on, it is
    // their turn to play, they hold one card that could be played, and they have not announced UNO. They can only
    // CallUno. Never a reason to draw: the card is playable, so neither the guided draw nor forcedAction() moves on.
    [[nodiscard]] bool mustDeclareUno(const PlayerId& player) const;
    // Whether the current player is the target of a penalty they may still answer: a Wild Draw Four (accept or
    // challenge) or a pending stack of penalties (accept or play a card on it, ADR 0028 and 0029).
    [[nodiscard]] bool awaitsPenaltyAnswer() const noexcept;
    // Whether `player` holds a card that may go on the pending stack of penalties (false when none is pending).
    [[nodiscard]] bool canStackOnPending(const PlayerId& player) const;
    // Whether DrawCard would be accepted from `player` right now: their turn, nothing else to answer, and the draw
    // rule lets them draw (ADR 0017).
    [[nodiscard]] bool canDraw(const PlayerId& player) const;
    // Whether Pass would be accepted from `player` right now: they hold a drawn card they may keep instead of playing.
    [[nodiscard]] bool canKeepDrawnCard(const PlayerId& player) const;
    // The one move the current player has left, if they have no choice (ADR 0017, guided draw only): draw when
    // nothing is playable, play a drawn card that is playable and plain. The engine only says what; the application
    // decides when, and applies it like any player action.
    [[nodiscard]] std::optional<PlayerAction> forcedAction() const;
    // Whether CallUno would have an effect for `player` right now: about to play with two cards, or already down to
    // one card and not announced (ADR 0018).
    [[nodiscard]] bool canCallUno(const PlayerId& player) const;
    // The UNO windows (SPEC §3, ADR 0018), oldest first: one per player who just left themselves with one card
    // without announcing it. A window stays open until that player announces, is caught, no longer holds exactly one
    // card, or the application closes it for lack of time (closeUnoWindow): playing or drawing does not close it.
    // While open, CatchUno{player} is accepted from any other seated player.
    [[nodiscard]] std::span<const PlayerId> unoWindows() const noexcept { return unoWindows_; }
    [[nodiscard]] bool hasUnoWindowOn(const PlayerId& player) const;
    // Closes the window on `player`, if any: its time ran out. The engine has no clock, so the application decides.
    void closeUnoWindow(const PlayerId& player);

    [[nodiscard]] bool operator==(const Round&) const = default;

private:
    Round(TurnOrder turnOrder, PlayerId dealer, std::vector<Hand> hands, DrawPile drawPile, DiscardPile discardPile);

    // Every check of a PlayCard, in the order of the errors: the card the action would play, or why it cannot.
    [[nodiscard]] std::expected<Card, DomainError> validatePlay(const PlayerId& actor, const PlayCard& action) const;
    // The target a Wild Draw Five needs (and that any other card refuses): a seated player other than `actor`.
    [[nodiscard]] std::expected<void, DomainError> checkTarget(const Card& card, const PlayCard& action,
                                                               const PlayerId& actor) const;
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    applyPlayCard(const PlayerId& actor, const PlayCard& action, RandomSource& random);
    // The penalty `card` inflicts once played on what is already owed: nothing for a card that is not a penalty card.
    [[nodiscard]] std::optional<Penalty> penaltyOfPlay(const Card& card, const PlayCard& action) const;
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

    // Makes `player` draw `count` cards as a penalty (fewer if the piles run short, SPEC §3), appending
    // DeckReshuffled if needed, then PenaltyCardsDrawn, to `events`.
    void drawPenalty(const PlayerId& player, std::size_t count, RandomSource& random, std::vector<DomainEvent>& events);
    // `winner` just played their last card: makes the target of the penalty it carries, if any, draw it (a last
    // penalty card still counts), scores the hands and moves to RoundOver, appending the events to `events`.
    void endRound(const PlayerId& winner, const std::optional<Penalty>& penalty, RandomSource& random,
                  std::vector<DomainEvent>& events);
    // The target of a stack of penalties accepts the pending total (a challenge is refused): ADR 0028 and 0029.
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    answerStack(const PlayerId& target, const RespondPenalty& action, std::size_t total, RandomSource& random);
    // Makes `target` draw the pending `total` and lose their turn.
    [[nodiscard]] std::vector<DomainEvent> acceptStack(const PlayerId& target, std::size_t total, RandomSource& random);

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
    PenaltyStacking stacking_{PenaltyStacking::Official};
    DrawRule drawRule_{DrawRule::Official};
    DrawAmount drawAmount_{DrawAmount::One};
    bool declareUnoToWin_{false};
    std::vector<Hand> hands_; // indexed by seat
    DrawPile drawPile_;
    DiscardPile discardPile_;
    std::optional<Color> currentColor_;
    TurnPhase phase_{AwaitingPlay{}};
    std::vector<bool> unoCalled_; // indexed by seat
    std::vector<PlayerId> unoWindows_;
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
