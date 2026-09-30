#pragma once

#include <compare> // IWYU pragma: keep (required by the defaulted operator<=>)
#include <string>

namespace uno::core {

// Strong type for the opaque, randomly generated identifier of a player (protocol `PlayerId`).
// uno_core only compares ids: generating them is the job of the session layer.
struct PlayerId {
    std::string value;

    auto operator<=>(const PlayerId&) const = default;
};

} // namespace uno::core
