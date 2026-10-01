#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace uno::testing {

// The value of an optional a test cannot go on without. An empty optional fails the test with an
// exception (reported by Catch2) instead of undefined behaviour.
template <typename T>
[[nodiscard]] T require(std::optional<T> value, std::string_view what = "a value")
{
    if (!value.has_value()) {
        throw std::runtime_error("expected " + std::string(what));
    }
    return std::move(*value);
}

} // namespace uno::testing
