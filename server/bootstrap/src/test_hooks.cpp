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
    // One knob for the whole pace of the game (ADR 0027): a card played, an effect, a drawn card and the pause after
    // every action take this long.
    if (const auto pace = environment("UNO_TEST_PACE_MS")) {
        const auto step = std::chrono::milliseconds(std::stoll(*pace));
        timeouts.playStep = step;
        timeouts.effectStep = step;
        timeouts.drawStep = step;
        timeouts.actionCooldown = step;
        timeouts.botThinkMin = step;
        timeouts.botThinkMax = step;
    }
#endif
    return timeouts;
}

} // namespace uno::bootstrap
