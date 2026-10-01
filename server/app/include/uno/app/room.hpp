#pragma once

#include "uno/app/identifiers.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace uno::app {

struct Member {
    core::PlayerId id;
    std::string nickname;
    bool ready{false};
    bool connected{true};
};

// A room: the people in it, the host's settings and, once started, the match. Seats are the members'
// positions in `members`, which is why nobody leaves a room mid-match (see Application).
// NOLINTNEXTLINE(bugprone-exception-escape): implicit noexcept moves; running out of memory ends the process
class Room {
public:
    Room(RoomCode roomCode, RoomSettings roomSettings, core::PlayerId firstMember, std::string nickname);

    [[nodiscard]] Member* find(const core::PlayerId& player);
    [[nodiscard]] const Member* find(const core::PlayerId& player) const;
    [[nodiscard]] bool isFull() const noexcept { return members.size() >= settings.maxPlayers; }
    [[nodiscard]] bool nicknameTaken(const std::string& nickname) const;
    [[nodiscard]] bool isHost(const core::PlayerId& player) const { return player == host; }

    void add(core::PlayerId player, std::string nickname);
    // Removes the member; the host role passes to the next connected member in seat order (SPEC §5).
    void remove(const core::PlayerId& player);

    [[nodiscard]] response::RoomView view() const;

    RoomCode code;
    RoomSettings settings;
    response::RoomPhase phase{response::RoomPhase::Lobby};
    std::vector<Member> members;
    core::PlayerId host;
    std::uint64_t version{0}; // bumped by every change a lobby member must be told about

    std::optional<core::Match> match;
    std::uint64_t stateVersion{0}; // bumped by every game.update
    std::set<core::PlayerId> readyForNextRound;
};

} // namespace uno::app
