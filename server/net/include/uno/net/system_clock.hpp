#pragma once

#include "uno/app/ports.hpp"

#include <chrono>
#include <cstdint>

namespace uno::net {

// What the protocol calls "server time" (EpochMillis): the wall clock read once at start-up, then advanced by a
// monotonic clock. The wall clock itself can be stepped back by a time synchronisation; a deadline such as
// `actionsOpenAt` (ADR 0027) never recedes, so a step of a few seconds would freeze a table for as long. Clients
// correct their offset from the `serverTime` of every update, so they never see the difference.
class SystemClock final : public app::Clock {
public:
    SystemClock();

    [[nodiscard]] std::int64_t nowMillis() const override;

private:
    std::int64_t startEpochMillis_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace uno::net
