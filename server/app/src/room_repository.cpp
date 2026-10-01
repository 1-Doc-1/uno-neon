#include "uno/app/room_repository.hpp"

#include <utility>

namespace uno::app {

Room* InMemoryRoomRepository::find(const RoomCode& code)
{
    const auto found = rooms_.find(code);
    return found == rooms_.end() ? nullptr : &found->second;
}

Room& InMemoryRoomRepository::add(Room room)
{
    const RoomCode code = room.code;
    return rooms_.insert_or_assign(code, std::move(room)).first->second;
}

void InMemoryRoomRepository::remove(const RoomCode& code)
{
    rooms_.erase(code);
}

} // namespace uno::app
