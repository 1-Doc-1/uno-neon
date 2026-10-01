#pragma once

#include "uno/app/identifiers.hpp"
#include "uno/app/room.hpp"

#include <cstddef>
#include <unordered_map>

namespace uno::app {

// Where rooms live (SPEC §7.3, Repository): the use cases never know whether it is memory or a database.
class RoomRepository {
public:
    virtual ~RoomRepository() = default;

    // The room with this code, or null. The pointer stays valid until the room is removed.
    [[nodiscard]] virtual Room* find(const RoomCode& code) = 0;
    virtual Room& add(Room room) = 0;
    virtual void remove(const RoomCode& code) = 0;
    [[nodiscard]] virtual std::size_t size() const = 0;

protected:
    RoomRepository() = default;
    RoomRepository(const RoomRepository&) = default;
    RoomRepository(RoomRepository&&) = default;
    RoomRepository& operator=(const RoomRepository&) = default;
    RoomRepository& operator=(RoomRepository&&) = default;
};

class InMemoryRoomRepository final : public RoomRepository {
public:
    [[nodiscard]] Room* find(const RoomCode& code) override;
    Room& add(Room room) override;
    void remove(const RoomCode& code) override;
    [[nodiscard]] std::size_t size() const override { return rooms_.size(); }

private:
    // Node-based: a Room never moves, so the pointers handed out stay valid.
    std::unordered_map<RoomCode, Room> rooms_;
};

} // namespace uno::app
