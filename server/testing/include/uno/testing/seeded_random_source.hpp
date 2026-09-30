#pragma once

#include "uno/core/random_source.hpp"

#include <cstdint>
#include <random>

namespace uno::testing {

// Deterministic RandomSource for tests and simulations only: the same seed gives the same
// sequence with every compiler and standard library (ADR 0008). Never used in production.
class SeededRandomSource final : public core::RandomSource {
public:
    explicit SeededRandomSource(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint32_t uniform(std::uint32_t upperExclusive) override;

private:
    std::mt19937_64 engine_;
};

} // namespace uno::testing
