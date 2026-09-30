#pragma once

#include <string_view>

namespace uno::core {

// Semantic version of the project, taken from the CMake `project()` declaration.
[[nodiscard]] std::string_view projectVersion() noexcept;

} // namespace uno::core
