#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// What the server may send (SPEC §8.4): replies to a request, then messages it pushes.
namespace uno::app::response {

// The request `replyTo` was accepted.
struct Ack {
    std::string replyTo;

    bool operator==(const Ack&) const = default;
};

// The request `replyTo` was rejected. `replyTo` is empty when the message could not be read at all.
struct Error {
    std::optional<std::string> replyTo;
    ErrorCode code{};
    std::string message; // technical English: the client shows its own French text for each code
    std::optional<IllegalMoveReason> reason;

    bool operator==(const Error&) const = default;
};

// Answer to session.hello.
struct Welcome {
    SessionToken sessionToken;
    core::PlayerId playerId;
    std::optional<RoomCode> resumedRoomCode;

    bool operator==(const Welcome&) const = default;
};

// lobby: waiting for players; inGame: a match is running; matchOver: waiting for a rematch.
enum class RoomPhase : std::uint8_t { Lobby, InGame, MatchOver };

struct RoomMember {
    core::PlayerId playerId;
    std::string nickname;
    std::uint8_t seat{};
    bool isHost{};
    bool isReady{};
    bool isConnected{};
    bool isBot{};

    bool operator==(const RoomMember&) const = default;
};

// State of a room as shown in the lobby: identical for every member.
struct RoomView {
    RoomCode code;
    RoomPhase phase{};
    RoomSettings settings;
    std::vector<RoomMember> players;

    bool operator==(const RoomView&) const = default;
};

struct RoomUpdate {
    std::uint64_t roomVersion{};
    RoomView room;

    bool operator==(const RoomUpdate&) const = default;
};

struct Reaction {
    core::PlayerId playerId;
    request::Emote emote{};

    bool operator==(const Reaction&) const = default;
};

enum class RoomClosedReason : std::uint8_t { Expired, Kicked, HostClosed };

struct RoomClosed {
    RoomClosedReason reason{};

    bool operator==(const RoomClosed&) const = default;
};

// What the room knows about a player that the engine does not: the part of a seat the engine cannot fill.
struct SeatInfo {
    std::string nickname;
    bool isConnected{};
    bool isBot{};
    bool isHost{};
    bool isReadyForNextRound{};

    bool operator==(const SeatInfo&) const = default;
};

// An open UNO window (ADR 0018): `targetId` holds one unannounced card. Only the target may announce until
// `graceEndsAt`; from then until `expiresAt` anybody else may catch them. Epoch milliseconds, server clock.
struct UnoWindowInfo {
    core::PlayerId targetId;
    std::int64_t graceEndsAt{};
    std::int64_t expiresAt{};

    bool operator==(const UnoWindowInfo&) const = default;
};

// The protocol PlayerView: the engine projection for one player, completed with what only the application
// knows. `seats` lists the same players as `game.players`, in the same order.
struct GameView {
    std::uint64_t stateVersion{};
    core::PlayerView game;
    std::vector<SeatInfo> seats;
    std::optional<std::int64_t> turnDeadline;      // epoch ms, server clock
    std::optional<std::int64_t> nextRoundDeadline; // epoch ms, server clock
    std::vector<UnoWindowInfo> unoWindows;         // oldest first
    RoomSettings settings;

    bool operator==(const GameView&) const = default;
};

// Sent to every player after each accepted action: the events to animate, then the full view to apply.
struct GameUpdate {
    std::int64_t serverTime{}; // lets the client correct its clock offset for the deadlines
    std::vector<core::ClientEvent> events;
    GameView view;

    bool operator==(const GameUpdate&) const = default;
};

using Message = std::variant<Ack, Error, Welcome, RoomUpdate, GameUpdate, Reaction, RoomClosed>;

} // namespace uno::app::response
