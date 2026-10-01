#pragma once

// JSON form of `game.update`: the events to animate and the PlayerView to apply (player-view.schema.json
// and client-event.schema.json).

#include "uno/app/server_message.hpp"

#include "json_reader.hpp"

namespace uno::net::detail {

[[nodiscard]] Json encodeSettings(const app::RoomSettings& settings);
[[nodiscard]] Parsed<app::RoomSettings> parseSettings(const Json& value);

[[nodiscard]] Json encodeGameUpdate(const app::response::GameUpdate& update);

// `payload` is the payload object of a game.update message.
[[nodiscard]] Parsed<app::response::GameUpdate> parseGameUpdate(const Json& payload);

} // namespace uno::net::detail
