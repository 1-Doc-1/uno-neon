#pragma once

#include <algorithm>
#include <vector>

namespace uno::core::detail {

// Internal helper: sorts `values` and reports whether any two compare equal. Shared by every
// uniqueness check in core (player ids, card ids); not part of the public API.
template <typename T>
[[nodiscard]] bool hasDuplicates(std::vector<T> values)
{
    std::ranges::sort(values);
    return std::ranges::adjacent_find(values) != values.end();
}

} // namespace uno::core::detail
