#include "uno/net/crypto_random_source.hpp"

#include <sodium.h>

namespace uno::net {

std::uint32_t CryptoRandomSource::uniform(std::uint32_t upperExclusive)
{
    // Rejection sampling inside libsodium: no modulo bias; returns 0 for an upper bound below 2.
    return randombytes_uniform(upperExclusive);
}

} // namespace uno::net
