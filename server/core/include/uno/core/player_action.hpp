#pragma once

#include "uno/core/card.hpp"

#include <optional>
#include <variant>

namespace uno::core {

// Closed set of intentions a player can send to Round::apply (ADR 0006; SPEC §7.3).

struct PlayCard {
    CardId cardId;
    // Required when the played card is a Wild, and forbidden otherwise.
    std::optional<Color> chosenColor;

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

using PlayerAction = std::variant<PlayCard, DrawCard, Pass, ChooseColor>;

} // namespace uno::core
