#pragma once

#include "uno/app/identifiers.hpp"
#include "uno/app/ports.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace uno::app {

// The clock of an open UNO window, which the engine does not have (ADR 0018).
struct UnoWindowTiming {
    core::PlayerId target;
    std::int64_t graceEndsAt{};
    std::int64_t expiresAt{};
    TimerHandle expiryTimer;
};

// What identifies one turn for its clock (ADR 0020): a broadcast that changes none of this (a counter-UNO, an
// announcement, a UNO window that ran out) does not give the player on turn a fresh clock. `turn` counts the turns
// the engine passed (`TurnChanged`), so a player who gets the turn again right after their own move (a Draw Two or a
// Skip with two players) starts a new clock too.
struct TurnKey {
    std::uint32_t round{};
    std::uint64_t turn{};
    core::PlayerId player;
    std::size_t phase{}; // index of the TurnPhase alternative

    [[nodiscard]] bool operator==(const TurnKey&) const = default;
};

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

    // Nicknames of players who left a running match: the engine still seats them when two players are left and one
    // of them leaves (forfeit), and the views still have to name that seat.
    std::unordered_map<std::string, std::string> formerNicknames;

    // Pending timers (see application_lifecycle.cpp). Cancelled when the room goes away.
    TimerHandle expiryTimer;
    TimerHandle turnTimer;
    std::optional<TurnKey> turnKey; // the turn the running turn timer was armed for
    std::uint64_t turnEpoch{0};     // bumped each time a turn timer is armed: an older one does nothing
    std::uint64_t turnCount{0};     // bumped each time the engine passes the turn
    TimerHandle nextRoundTimer;
    TimerHandle forcedActionTimer; // plays the move a player has no choice about (guided draw)
    std::unordered_map<std::string, TimerHandle> graceTimers; // by player id: disconnected, waiting to come back
    std::optional<std::int64_t> turnDeadline;                 // epoch ms, shown in the views
    std::int64_t actionsOpenAt{0}; // epoch ms: turn actions are refused before (ADR 0027); never moves backwards
    std::optional<std::int64_t> nextRoundDeadline;
    std::vector<UnoWindowTiming> unoWindows; // mirrors Round::unoWindows(), with times

    [[nodiscard]] const UnoWindowTiming* findUnoWindow(const core::PlayerId& target) const;
};

} // namespace uno::app
