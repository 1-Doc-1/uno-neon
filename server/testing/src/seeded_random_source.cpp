#include "uno/testing/seeded_random_source.hpp"

#include <cstdint>
#include <limits>

namespace uno::testing {

SeededRandomSource::SeededRandomSource(std::uint64_t seed) noexcept : engine_{seed} {}

std::uint32_t SeededRandomSource::uniform(std::uint32_t upperExclusive)
{
    if (upperExclusive <= 1) {
        return 0;
    }
    // Rejection sampling, written by hand because std::uniform_int_distribution is
    // implementation-defined (ADR 0008). Draws below `threshold` (= 2^64 mod bound) are
    // rejected: the accepted range then holds a whole number of copies of [0, bound),
    // so `draw % bound` has no modulo bias.
    const std::uint64_t bound = upperExclusive;
    const std::uint64_t threshold = (std::numeric_limits<std::uint64_t>::max() - bound + 1) % bound;
    std::uint64_t draw = engine_();
    while (draw < threshold) {
        draw = engine_();
    }
    return static_cast<std::uint32_t>(draw % bound);
}

} // namespace uno::testing
