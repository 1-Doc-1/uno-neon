#pragma once

#include "uno/core/random_source.hpp"

#include <cstdint>

namespace uno::net {

// Production source of randomness (SPEC §7.4): libsodium's cryptographically secure generator. It
// shuffles the deck and generates session tokens and room codes, so none of them can be predicted.
// initializeCryptoRuntime() must have succeeded first.
class CryptoRandomSource final : public core::RandomSource {
public:
    [[nodiscard]] std::uint32_t uniform(std::uint32_t upperExclusive) override;
};

} // namespace uno::net
