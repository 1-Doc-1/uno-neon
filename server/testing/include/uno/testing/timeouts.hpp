#pragma once

#include "uno/app/application.hpp"

#include <chrono>

namespace uno::testing {

// The delays of production, except that showing an action takes no time: scenario tests play one action after the
// other at the same instant. Tests of the pace (ADR 0027) pass `app::Timeouts{}` to get the real one.
inline app::Timeouts withoutPresentationDelay()
{
    app::Timeouts timeouts;
    timeouts.playStep = std::chrono::milliseconds(0);
    timeouts.effectStep = std::chrono::milliseconds(0);
    timeouts.drawStep = std::chrono::milliseconds(0);
    timeouts.actionCooldown = std::chrono::milliseconds(0);
    timeouts.botThinkMin = std::chrono::milliseconds(0);
    timeouts.botThinkMax = std::chrono::milliseconds(0);
    return timeouts;
}

} // namespace uno::testing
