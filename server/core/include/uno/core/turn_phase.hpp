#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

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
// who must accept the penalty or challenge the card's legality (RespondPenalty). `wildDrawFourPlayer`
// and `wasLegal` are recorded once, when the card is played, and never recomputed: the player who
// posed it can no longer act while this phase lasts, so their hand cannot have changed in between.
struct AwaitingPenaltyResponse {
    PlayerId wildDrawFourPlayer;
    bool wasLegal{};

    bool operator==(const AwaitingPenaltyResponse&) const = default;
};

using TurnPhase = std::variant<AwaitingPlay, AwaitingDrawnCardDecision, AwaitingColorChoice, AwaitingPenaltyResponse>;

} // namespace uno::core
