#pragma once

#include "uno/app/ports.hpp"

#include <cstdint>

namespace uno::net {

// The wall clock: what the protocol calls "server time" (EpochMillis).
class SystemClock final : public app::Clock {
public:
    [[nodiscard]] std::int64_t nowMillis() const override;
};

} // namespace uno::net
