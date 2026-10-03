#include "uno/app/room.hpp"

#include "uno/app/nickname.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>

namespace uno::app {

Room::Room(RoomCode roomCode, RoomSettings roomSettings, core::PlayerId firstMember, std::string nickname)
    : code(std::move(roomCode)), settings(roomSettings), host(firstMember)
{
    members.push_back(Member{.id = std::move(firstMember), .nickname = std::move(nickname)});
}

Member* Room::find(const core::PlayerId& player)
{
    const auto found = std::ranges::find(members, player, &Member::id);
    return found == members.end() ? nullptr : &*found;
}

const UnoWindowTiming* Room::findUnoWindow(const core::PlayerId& target) const
{
    const auto found = std::ranges::find(unoWindows, target, &UnoWindowTiming::target);
    return found == unoWindows.end() ? nullptr : &*found;
}

const Member* Room::find(const core::PlayerId& player) const
{
    const auto found = std::ranges::find(members, player, &Member::id);
    return found == members.end() ? nullptr : &*found;
}

bool Room::nicknameTaken(const std::string& nickname) const
{
    const std::string key = nicknameKey(nickname);
    return std::ranges::any_of(members, [&key](const Member& member) { return nicknameKey(member.nickname) == key; });
}

void Room::add(core::PlayerId player, std::string nickname)
{
    members.push_back(Member{.id = std::move(player), .nickname = std::move(nickname)});
}

void Room::remove(const core::PlayerId& player)
{
    const auto leaving = std::ranges::find(members, player, &Member::id);
    if (leaving == members.end()) {
        return;
    }
    const auto leavingSeat = static_cast<std::size_t>(std::distance(members.begin(), leaving));
    members.erase(leaving);
    if (members.empty() || host != player) {
        return;
    }
    // The next connected member after the leaving seat, wrapping around; anyone if nobody is connected.
    for (std::size_t offset = 0; offset < members.size(); ++offset) {
        const Member& candidate = members.at((leavingSeat + offset) % members.size());
        if (candidate.connected) {
            host = candidate.id;
            return;
        }
    }
    host = members.front().id;
}

response::RoomView Room::view() const
{
    response::RoomView roomView{.code = code, .phase = phase, .settings = settings, .players = {}};
    for (std::size_t seat = 0; seat < members.size(); ++seat) {
        const Member& member = members.at(seat);
        const bool isRoomHost = isHost(member.id);
        roomView.players.push_back(response::RoomMember{
            .playerId = member.id,
            .nickname = member.nickname,
            .seat = static_cast<std::uint8_t>(seat),
            .isHost = isRoomHost,
            // Starting the match is the host's way of saying they are ready.
            .isReady = isRoomHost || member.ready,
            .isConnected = member.connected,
            .isBot = false,
        });
    }
    return roomView;
}

} // namespace uno::app
