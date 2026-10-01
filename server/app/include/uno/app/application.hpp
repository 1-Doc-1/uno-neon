#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/ports.hpp"
#include "uno/app/room.hpp"
#include "uno/app/room_repository.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uno::app {

// The use cases of the server: sessions, rooms and (from step 2.4) matches. It reacts to what the
// transport reports and answers through the MessageSink; it knows neither sockets nor JSON.
//
// Every request is handled the same way: a handler validates, changes state and queues the messages
// it wants to send, then the dispatcher sends the `ack` (or the `error`) first and the queued messages
// after, so a client always learns that its request succeeded before it sees the consequences.
class Application final : public ConnectionHandler {
public:
    // All dependencies are injected and must outlive the application.
    Application(MessageSink& sink, RoomRepository& rooms, core::RandomSource& random, const Clock& clock);

    void onConnected(ConnectionId connection) override;
    void onRequest(ConnectionId connection, request::Envelope request) override;
    void onDisconnected(ConnectionId connection) override;

private:
    struct Failure {
        ErrorCode code{};
        std::string message;
        std::optional<IllegalMoveReason> reason;
    };
    enum class Reply : std::uint8_t { Ack, None };
    using Outcome = std::expected<Reply, Failure>;

    // A player's identity, which outlives their connections.
    struct Session {
        SessionToken token;
        core::PlayerId playerId;
        std::optional<ConnectionId> connection;
        std::optional<RoomCode> room;
    };

    [[nodiscard]] static std::unexpected<Failure> fail(ErrorCode code, std::string message,
                                                       std::optional<IllegalMoveReason> reason = std::nullopt);

    Outcome handle(ConnectionId connection, const request::Hello& request);
    Outcome handle(ConnectionId connection, const request::CreateRoom& request);
    Outcome handle(ConnectionId connection, const request::JoinRoom& request);
    Outcome handle(ConnectionId connection, const request::LeaveRoom& request);
    Outcome handle(ConnectionId connection, const request::UpdateSettings& request);
    Outcome handle(ConnectionId connection, const request::SetReady& request);
    Outcome handle(ConnectionId connection, const request::Kick& request);
    Outcome handle(ConnectionId connection, const request::AddBot& request);
    Outcome handle(ConnectionId connection, const request::SendReaction& request);
    // Requests of a match, available from step 2.4.
    template <typename Request>
    Outcome handle(ConnectionId connection, const Request& request);

    // The session behind a connection that said hello, or SESSION_REQUIRED.
    [[nodiscard]] std::expected<Session*, Failure> sessionOf(ConnectionId connection);
    // The room of a session, or NOT_IN_ROOM.
    [[nodiscard]] std::expected<Room*, Failure> roomOf(const Session& session);
    // The room of a session whose player is its host, in the lobby: what the host's lobby commands need.
    [[nodiscard]] std::expected<Room*, Failure> hostedLobbyOf(const Session& session);

    void queue(const core::PlayerId& player, response::Message message);
    // Tells every member the lobby changed.
    void broadcastRoom(Room& room);
    void flush();

    MessageSink* sink_;
    RoomRepository* rooms_;
    core::RandomSource* random_;
    const Clock* clock_;

    std::unordered_map<std::string, Session> sessions_; // by player id
    std::unordered_map<std::string, std::string> playerOfToken_;
    std::unordered_map<ConnectionId, std::string> playerOfConnection_;
    std::unordered_map<std::string, std::int64_t> lastReactionAt_; // by player id
    std::vector<std::pair<ConnectionId, response::Message>> outbox_;
};

} // namespace uno::app
