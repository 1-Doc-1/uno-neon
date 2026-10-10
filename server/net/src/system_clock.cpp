#include "uno/net/system_clock.hpp"

#include <chrono>

namespace uno::net {

SystemClock::SystemClock()
    : startEpochMillis_(
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
              .count()),
      start_(std::chrono::steady_clock::now())
{
}

std::int64_t SystemClock::nowMillis() const
{
    const auto elapsed = std::chrono::steady_clock::now() - start_;
    return startEpochMillis_ + std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

} // namespace uno::net
