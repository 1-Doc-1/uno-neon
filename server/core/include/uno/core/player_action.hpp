#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace uno::core {

// Closed set of intentions a player can send to Round::apply (ADR 0006; SPEC §7.3).

struct PlayCard {
    CardId cardId;
    // Required when the played card is a Wild, and forbidden otherwise.
    std::optional<Color> chosenColor;
    // Required when the played card is a Wild Draw Five (any other player of the round), forbidden otherwise (ADR
    // 0028).
    // The braces are not redundant: they keep `PlayCard{.cardId = ..., .chosenColor = ...}` free of the missing-field
    // warning
    std::optional<PlayerId> target{}; // NOLINT(readability-redundant-member-init)

    bool operator==(const PlayCard&) const = default;
};

struct DrawCard {
    bool operator==(const DrawCard&) const = default;
};

// Only legal right after drawing an unplayable card (phase AwaitingDrawnCardDecision).
struct Pass {
    bool operator==(const Pass&) const = default;
};

// Only legal while awaiting the initial color choice after a Wild is flipped as the first card
// (phase AwaitingColorChoice; ADR 0007).
struct ChooseColor {
    Color color{};

    bool operator==(const ChooseColor&) const = default;
};

enum class PenaltyResponse : std::uint8_t { Accept, Challenge };

// Only legal while targeted by a pending draw penalty (phase AwaitingPenaltyResponse) — in step
// 1.3b, always a Wild Draw Four's challenge window.
struct RespondPenalty {
    PenaltyResponse response{};

    bool operator==(const RespondPenalty&) const = default;
};

// Announces UNO, out of turn if need be (SPEC §3). Accepted in two situations only: the player is
// about to play with exactly two cards in hand, or the UNO window is open on them (Round::unoWindow).
// In any other case it is a harmless no-op: no events, no state change.
struct CallUno {
    bool operator==(const CallUno&) const = default;
};

// Catches a player who left themselves with one card without announcing it. Legal only while the
// UNO window is open on `target` (see Round::unoWindow).
struct CatchUno {
    PlayerId target;

    bool operator==(const CatchUno&) const = default;
};

using PlayerAction = std::variant<PlayCard, DrawCard, Pass, ChooseColor, RespondPenalty, CallUno, CatchUno>;

} // namespace uno::core
