#include "uno/bootstrap/test_hooks.hpp"

#include "uno/net/crypto_random_source.hpp"

#ifdef UNO_ENABLE_TEST_HOOKS
#include "uno/testing/seeded_random_source.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#endif

namespace uno::bootstrap {

std::unique_ptr<core::RandomSource> makeRandomSource([[maybe_unused]] const EnvironmentLookup& environment)
{
#ifdef UNO_ENABLE_TEST_HOOKS
    if (const auto seed = environment("UNO_TEST_SEED")) {
        spdlog::warn("UNO_TEST_SEED is set: randomness is NOT secure (test build)");
        return std::make_unique<testing::SeededRandomSource>(std::stoull(*seed));
    }
#endif
    return std::make_unique<net::CryptoRandomSource>();
}

app::Timeouts makeTimeouts([[maybe_unused]] const EnvironmentLookup& environment)
{
    app::Timeouts timeouts;
#ifdef UNO_ENABLE_TEST_HOOKS
    if (const auto grace = environment("UNO_TEST_RECONNECT_GRACE_MS")) {
        timeouts.reconnectGrace = std::chrono::milliseconds(std::stoll(*grace));
    }
    if (const auto step = environment("UNO_TEST_DRAW_STEP_MS")) {
        timeouts.drawStep = std::chrono::milliseconds(std::stoll(*step));
    }
#endif
    return timeouts;
}

} // namespace uno::bootstrap
