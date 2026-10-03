#pragma once

#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace uno::core {

// The game part of the protocol's PlayerView (protocol/schema/player-view.schema.json): everything one
// player is allowed to know about a match, and what they may do next. It is the ONLY representation of
// a match that ever leaves the engine towards a client (SPEC §7.3, "Projection"; security invariant 2):
// it holds no other player's cards, no draw pile order and no random seed, so a new field cannot leak
// them by accident. The network layer adds what the engine does not know (nickname, connection state,
// host, deadlines, state version, room settings) when it serializes this.
// Fields come in protocol order; two views compare equal when a player cannot tell them apart.

enum class ViewPhase : std::uint8_t {
    AwaitingPlay,
    AwaitingDrawnCardDecision,
    AwaitingPenaltyResponse,
    AwaitingColorChoice,
    RoundOver,
    MatchOver,
};

struct PenaltyResponseOptions {
    std::size_t amount{};
    bool canChallenge{};
    bool canStack{}; // always false until the stacking option exists (step 1.6)

    bool operator==(const PenaltyResponseOptions&) const = default;
};

// What the viewer holds and may do right now. Every "can..." answer is precomputed by the engine so
// that a client never has to know the rules.
struct MyState {
    PlayerId playerId;
    std::vector<Card> hand;
    std::vector<CardId> playableCardIds;
    bool canDraw{};
    bool canKeepDrawnCard{}; // Pass: keep the card just drawn instead of playing it
    bool mustDeclareUno{};   // stuck on the last card until UNO is announced (house rule, ADR 0019)
    bool canCallUno{};
    bool canChooseColor{};
    std::optional<PenaltyResponseOptions> penaltyResponse;

    bool operator==(const MyState&) const = default;
};

// Public information about one player (the viewer included): how many cards, never which.
struct SeatView {
    PlayerId playerId;
    std::size_t seat{};
    std::size_t cardCount{};
    std::uint32_t score{};
    bool hasCalledUno{};

    bool operator==(const SeatView&) const = default;
};

struct RevealedHand {
    PlayerId playerId;
    std::vector<Card> cards;

    bool operator==(const RevealedHand&) const = default;
};

// Present once a round is over: at that point every remaining hand is public, since they were counted
// into the score (SPEC §3).
struct RoundResult {
    PlayerId winnerId;
    std::uint32_t points{};
    std::vector<RevealedHand> revealedHands;

    bool operator==(const RoundResult&) const = default;
};

struct PlayerView {
    ViewPhase phase{};
    MyState me;
    std::vector<SeatView> players; // by seat
    PlayerId currentPlayerId;
    Direction direction{};
    std::optional<Color> currentColor; // empty only while the first color is being chosen
    Card discardTop;
    std::size_t drawPileCount{};
    std::size_t pendingDraw{}; // draw penalty waiting for an answer, 0 if none
    std::uint32_t round{};
    std::optional<RoundResult> roundResult;
    std::optional<PlayerId> matchWinnerId;

    bool operator==(const PlayerView&) const = default;
};

// Projects a round for `viewer` (SPEC §7.2). `progress` supplies what the round does not know: scores, the
// round number and the match winner. UnknownPlayer if `viewer` is not seated.
[[nodiscard]] std::expected<PlayerView, DomainError> project(const Round& round, const PlayerId& viewer,
                                                             const MatchProgress& progress);

// Same, for the current round of `match`.
[[nodiscard]] std::expected<PlayerView, DomainError> project(const Match& match, const PlayerId& viewer);

} // namespace uno::core
