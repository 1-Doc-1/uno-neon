#include "uno/net/system_clock.hpp"

#include <chrono>

namespace uno::net {

std::int64_t SystemClock::nowMillis() const
{
    const auto sinceEpoch = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch).count();
}

} // namespace uno::net
