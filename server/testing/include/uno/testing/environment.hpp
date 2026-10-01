#pragma once

#include <optional>
#include <string>

namespace uno::testing {

// Value of an environment variable, if set. Lets a test run be tuned without recompiling (for example
// the number of simulated matches).
[[nodiscard]] std::optional<std::string> environmentVariable(const char* name);

} // namespace uno::testing
