#pragma once

#include "uno/core/round.hpp"

namespace uno::testing {

// Checks structural invariants of a Round that must hold after every successful start()/apply()
// call. Used by the engine test suite and, from step 1.8, by the massive random simulation.
void requireRoundInvariants(const core::Round& round);

} // namespace uno::testing
