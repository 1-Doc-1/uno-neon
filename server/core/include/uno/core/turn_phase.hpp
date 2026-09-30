#pragma once

#include "uno/core/card.hpp"

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

using TurnPhase = std::variant<AwaitingPlay, AwaitingDrawnCardDecision, AwaitingColorChoice>;

} // namespace uno::core
