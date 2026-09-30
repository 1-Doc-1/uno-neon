#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

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

using DomainEvent = std::variant<RoundStarted, CardPlayed, CardsDrawn, PenaltyCardsDrawn, DeckReshuffled, TurnPassed,
                                 PlayerSkipped, DirectionReversed, ColorChosen, TurnChanged>;

} // namespace uno::core
