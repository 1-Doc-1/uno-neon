#pragma once

#include "uno/core/client_event.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"

#include <span>

namespace uno::testing {

// Security invariant 2 (CLAUDE.md), checked on a concrete state: the view of `viewer` exposes no card
// but their own hand and the top of the discard pile (plus, once the round is over, the hands the
// rules reveal), and it counts the other players' and the draw pile's cards without showing them.
void requireViewLeaksNothing(const core::Round& round, const core::PlayerView& view, const core::PlayerId& viewer);

// Same invariant for the events projected for `viewer` right after an action: a card appears in them only
// when the viewer may see it (their own drawn cards, the card just played, a hand revealed by a challenge to
// the challenger alone), and each event carries exactly the optional fields its viewer is entitled to.
void requireEventsLeakNothing(std::span<const core::ClientEvent> events, const core::Round& roundAfter,
                              const core::PlayerId& viewer);

} // namespace uno::testing
