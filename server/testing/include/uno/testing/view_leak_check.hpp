#pragma once

#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"

namespace uno::testing {

// Security invariant 2 (CLAUDE.md), checked on a concrete state: the view of `viewer` exposes no card
// but their own hand and the top of the discard pile (plus, once the round is over, the hands the
// rules reveal), and it counts the other players' and the draw pile's cards without showing them.
void requireViewLeaksNothing(const core::Round& round, const core::PlayerView& view, const core::PlayerId& viewer);

} // namespace uno::testing
