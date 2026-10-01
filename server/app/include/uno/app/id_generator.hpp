#pragma once

#include "uno/app/identifiers.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"

#include <functional>

namespace uno::app {

// Secrets and identifiers drawn from an injected RandomSource: in production the cryptographic one
// (SPEC §7.4, security invariant 4), never std::mt19937.

// 22 base64url characters = 132 random bits (the protocol asks for at least 128).
[[nodiscard]] SessionToken generateSessionToken(core::RandomSource& random);

// "p_" followed by 10 random base64url characters: opaque, never a seat number.
[[nodiscard]] core::PlayerId generatePlayerId(core::RandomSource& random);

// 6 characters of the unambiguous alphabet (no I, L, O, 0, 1), redrawn until `isTaken` says the code is free.
[[nodiscard]] RoomCode generateRoomCode(core::RandomSource& random,
                                        const std::function<bool(const RoomCode&)>& isTaken);

} // namespace uno::app
