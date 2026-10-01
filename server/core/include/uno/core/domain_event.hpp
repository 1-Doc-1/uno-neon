#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace uno::core {

// Closed set of facts a round can report after start() or apply() (ADR 0006; SPEC §8.5). Names
// and shapes stay close to the protocol's events so the future PlayerView projection (step 1.7)
// can be a direct, one-to-one translation.

struct RoundStarted {
    PlayerId dealer;
    Card firstCard;

    bool operator==(const RoundStarted&) const = default;
};

struct CardPlayed {
    PlayerId player;
    CardId cardId;

    bool operator==(const CardPlayed&) const = default;
};

// A voluntary draw (the "draw or pass" turn action).
struct CardsDrawn {
    PlayerId player;
    std::vector<CardId> cards;

    bool operator==(const CardsDrawn&) const = default;
};

// A forced draw from a penalty effect (Draw Two in 1.3a; Wild Draw Four from 1.3b).
struct PenaltyCardsDrawn {
    PlayerId player;
    std::vector<CardId> cards;

    bool operator==(const PenaltyCardsDrawn&) const = default;
};

// The draw pile ran out and was refilled by reshuffling the discard pile (its top card excepted).
struct DeckReshuffled {
    bool operator==(const DeckReshuffled&) const = default;
};

struct TurnPassed {
    PlayerId player;

    bool operator==(const TurnPassed&) const = default;
};

// The named player's turn was skipped: a Skip, or a Reverse acting as a Skip with two players.
struct PlayerSkipped {
    PlayerId skippedPlayer;

    bool operator==(const PlayerSkipped&) const = default;
};

// The direction of play changed (a Reverse with three or more players).
struct DirectionReversed {
    bool operator==(const DirectionReversed&) const = default;
};

struct ColorChosen {
    PlayerId player;
    Color color{};

    bool operator==(const ColorChosen&) const = default;
};

// The current player changed. Emitted once, after every other event of the same effect chain.
struct TurnChanged {
    PlayerId player;

    bool operator==(const TurnChanged&) const = default;
};

// Verdict of a Wild Draw Four challenge (SPEC §3). `penaltyAmount` is the amount decided by the
// verdict (4 if the poser bluffed, 6 if the challenge failed) — kept apart from the `cards` field
// of the `PenaltyCardsDrawn` that follows, since a near-empty draw and discard pile (SPEC §5) can
// make the actual draw smaller than the decided amount. `revealedHand` is the challenged player's
// hand at the moment they played the card; ADR 0007 restricts it, in the projection (step 1.7), to
// the challenger alone — at this level the event carries the full information.
struct ChallengeResolved {
    PlayerId challenger;
    PlayerId challenged;
    bool wasBluff{};
    PlayerId penalizedPlayer;
    std::size_t penaltyAmount{};
    std::vector<Card> revealedHand;

    bool operator==(const ChallengeResolved&) const = default;
};

using DomainEvent = std::variant<RoundStarted, CardPlayed, CardsDrawn, PenaltyCardsDrawn, DeckReshuffled, TurnPassed,
                                 PlayerSkipped, DirectionReversed, ColorChosen, TurnChanged, ChallengeResolved>;

} // namespace uno::core
