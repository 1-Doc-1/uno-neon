#pragma once

#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"

#include <vector>

namespace uno::testing {

// Every action the current player may legally take right now (never empty while the round is in
// progress, empty once it is over). Wilds are listed once per color. Out-of-turn actions
// (CallUno, CatchUno) are not included. Lets simulations and tests play without re-implementing the rules.
[[nodiscard]] std::vector<core::PlayerAction> legalActionsOfCurrentPlayer(const core::Round& round);

} // namespace uno::testing
