#pragma once

namespace uno::net {

// Initialises libsodium. Must succeed before any cryptographic random number is drawn.
// Safe to call several times.
[[nodiscard]] bool initializeCryptoRuntime() noexcept;

} // namespace uno::net
