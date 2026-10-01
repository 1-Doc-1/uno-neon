#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

#include <cstdint>
#include <variant>

namespace uno::core {

// Closed set of states a round's turn can be in (ADR 0006; SPEC §7.3). Each phase only accepts
// its own actions; Round::apply rejects anything else with DomainError::InvalidPhase, so invalid
// combinations are never representable in the first place.

struct AwaitingPlay {
    bool operator==(const AwaitingPlay&) const = default;
};

// The current player just drew a playable card and must play that exact card or pass.
struct AwaitingDrawnCardDecision {
    CardId drawnCard;

    bool operator==(const AwaitingDrawnCardDecision&) const = default;
};

// A Wild was flipped as the first card of the round: the first player must choose a color before
// playing (ADR 0007).
struct AwaitingColorChoice {
    bool operator==(const AwaitingColorChoice&) const = default;
};

// A Wild Draw Four was just played: the turn has already moved to the targeted player (SPEC §3),
// who must accept the penalty or challenge the card's legality (RespondPenalty). `wasLegal` is
// computed once, when the card is played, because legality is judged against the color that was
// current *before* the poser chose a new one: at challenge time `currentColor_` is already the
// chosen color, so recomputing then would give a wrong verdict (no red in hand but a blue card,
// red current and blue chosen: legal, yet it would look illegal against blue).
struct AwaitingPenaltyResponse {
    PlayerId wildDrawFourPlayer;
    bool wasLegal{};

    bool operator==(const AwaitingPenaltyResponse&) const = default;
};

// A player played their last card (SPEC §3). `points` is what the winner scores: the value of every
// card left in the other hands, counted after the penalty draw of a last Draw Two or Wild Draw Four.
// Final for this round: every action is rejected with DomainError::InvalidPhase.
struct RoundOver {
    PlayerId winner;
    std::uint32_t points{};

    bool operator==(const RoundOver&) const = default;
};

using TurnPhase =
    std::variant<AwaitingPlay, AwaitingDrawnCardDecision, AwaitingColorChoice, AwaitingPenaltyResponse, RoundOver>;

} // namespace uno::core
