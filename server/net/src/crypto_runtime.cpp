#include "uno/net/crypto_runtime.hpp"

#include <sodium.h>

namespace uno::net {

bool initializeCryptoRuntime() noexcept
{
    // sodium_init() returns 0 on first success, 1 if already initialised, -1 on failure.
    return sodium_init() >= 0;
}

} // namespace uno::net
