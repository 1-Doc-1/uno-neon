#pragma once

#include "uno/app/application.hpp"
#include "uno/core/random_source.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace uno::bootstrap {

// Reads one environment variable; injected so the choices below can be tested without touching the process environment.
using EnvironmentLookup = std::function<std::optional<std::string>(const char*)>;

// The source of randomness of the whole server: cryptographic, always (SPEC §7.4). Only a binary built with
// UNO_ENABLE_TEST_HOOKS can be given a seed for deterministic end-to-end tests; in any other build the variable is not
// read, and its name is not even in the binary.
[[nodiscard]] std::unique_ptr<core::RandomSource> makeRandomSource(const EnvironmentLookup& environment);

// The timeouts of the application: the defaults, always. A test build may shorten the reconnection grace so that an
// end-to-end test can watch a player forfeit without waiting a minute, and the pace of the drawn cards.
[[nodiscard]] app::Timeouts makeTimeouts(const EnvironmentLookup& environment);

} // namespace uno::bootstrap
