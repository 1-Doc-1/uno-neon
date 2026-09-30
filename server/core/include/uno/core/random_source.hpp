#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace uno::core {

// Injected source of randomness: uno_core never uses a global generator (ADR 0006).
class RandomSource {
public:
    virtual ~RandomSource() = default;

    // Uniformly distributed value in [0, upperExclusive), without modulo bias.
    // Returns 0 when upperExclusive is 0 or 1, like libsodium's randombytes_uniform().
    [[nodiscard]] virtual std::uint32_t uniform(std::uint32_t upperExclusive) = 0;

protected:
    RandomSource() = default;
    RandomSource(const RandomSource&) = default;
    RandomSource(RandomSource&&) = default;
    RandomSource& operator=(const RandomSource&) = default;
    RandomSource& operator=(RandomSource&&) = default;
};

// Fisher-Yates shuffle. Hand-written rather than std::shuffle, whose algorithm is
// implementation-defined: the same seed must give the same order on every platform (ADR 0008).
template <typename T, std::size_t Extent>
void shuffle(std::span<T, Extent> elements, RandomSource& random)
{
    if (elements.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error{"shuffle: too many elements for RandomSource::uniform"};
    }
    const auto first = elements.begin();
    for (auto remaining = static_cast<std::uint32_t>(elements.size()); remaining > 1; --remaining) {
        const auto picked = random.uniform(remaining);
        std::ranges::iter_swap(first + (remaining - 1), first + picked);
    }
}

} // namespace uno::core
