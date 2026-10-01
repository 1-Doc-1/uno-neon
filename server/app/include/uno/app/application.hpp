#pragma once

#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/identifiers.hpp"
#include "uno/app/ports.hpp"
#include "uno/app/room.hpp"
#include "uno/app/room_repository.hpp"
#include "uno/app/server_message.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uno::app {

// How long the application waits before it acts on silence (SPEC §5, §9.3).
struct Timeouts {
    std::chrono::milliseconds reconnectGrace{std::chrono::seconds(60)};     // a disconnected player is removed
    std::chrono::milliseconds lobbyInactivity{std::chrono::minutes(15)};    // an idle lobby closes
    std::chrono::milliseconds matchOverInactivity{std::chrono::minutes(5)}; // a finished match closes
    std::chrono::milliseconds sessionIdle{std::chrono::minutes(10)};        // a session nobody uses is forgotten
    std::chrono::milliseconds nextRound{std::chrono::seconds(30)};          // the next round starts anyway
};

// The use cases of the server: sessions, rooms and (from step 2.4) matches. It reacts to what the
// transport reports and answers through the MessageSink; it knows neither sockets nor JSON.
//
// Every request is handled the same way: a handler validates, changes state and queues the messages
// it wants to send, then the dispatcher sends the `ack` (or the `error`) first and the queued messages
// after, so a client always learns that its request succeeded before it sees the consequences.
class Application final : public ConnectionHandler {
public:
    // All dependencies are injected and must outlive the application.
    Application(MessageSink& sink, RoomRepository& rooms, core::RandomSource& random, const Clock& clock,
                Scheduler& scheduler, Timeouts timeouts = {});

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
        TimerHandle idleTimer; // forgets the session when nobody comes back
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
    Outcome handle(ConnectionId connection, const request::StartMatch& request);
    Outcome handle(ConnectionId connection, const request::Rematch& request);
    Outcome handle(ConnectionId connection, const request::ReadyForNextRound& request);
    Outcome handle(ConnectionId connection, const request::PlayCard& request);
    Outcome handle(ConnectionId connection, const request::DrawCard& request);
    Outcome handle(ConnectionId connection, const request::Pass& request);
    Outcome handle(ConnectionId connection, const request::ChooseColor& request);
    Outcome handle(ConnectionId connection, const request::RespondPenalty& request);
    Outcome handle(ConnectionId connection, const request::CallUno& request);
    Outcome handle(ConnectionId connection, const request::CatchUno& request);

    [[nodiscard]] static Outcome engineFailure(core::DomainError error);

    // Applies a player action to the match of the sender's room and tells everyone what happened.
    Outcome play(ConnectionId connection, const core::PlayerAction& action);
    // Deals the first round of a new match to the members of the room, in seat order.
    Outcome startMatch(Room& room);

    // The session behind a connection that said hello, or SESSION_REQUIRED.
    [[nodiscard]] std::expected<Session*, Failure> sessionOf(ConnectionId connection);
    // The room of a session, or NOT_IN_ROOM.
    [[nodiscard]] std::expected<Room*, Failure> roomOf(const Session& session);
    // The room of a session whose player is its host, in the lobby: what the host's lobby commands need.
    [[nodiscard]] std::expected<Room*, Failure> hostedLobbyOf(const Session& session);

    void queue(const core::PlayerId& player, response::Message message);
    // Tells every member the lobby changed.
    void broadcastRoom(Room& room);
    // Sends every member the new state of the match: the engine events (projected for each of them) followed by
    // `extraEvents` (connections, which the engine ignores), and their own view.
    void broadcastGame(Room& room, std::span<const core::DomainEvent> events,
                       const std::vector<core::ClientEvent>& extraEvents = {});
    [[nodiscard]] static response::GameView viewOf(const Room& room, const core::Match& match,
                                                   const core::PlayerId& viewer);
    void flush();

    // ---- membership ----
    // Takes a player out of their room for good, and out of the match if one is running (leave, kick, or a grace
    // period that ran out): hands the host role over, tells the others, closes the room when nobody is left.
    void removeFromRoom(Room& room, const core::PlayerId& player);
    // Applies the consequences of a change of the match: the room phase, the broadcast, the timers.
    void afterMatchChange(Room& room, std::span<const core::DomainEvent> events,
                          const std::vector<core::ClientEvent>& extraEvents = {});
    // Starts the next round when every connected player is ready (or the deadline passed, with `force`).
    void startNextRoundIfDue(Room& room, bool force);

    // ---- timers (application_lifecycle.cpp) ----
    void scheduleRoomExpiry(Room& room);
    void armGameTimers(Room& room);
    void startGraceTimer(Room& room, const core::PlayerId& player);
    void scheduleSessionIdle(Session& session);
    // Cancels the timers of a room, forgets it and frees its members; `reason` tells them why, if given.
    void destroyRoom(Room& room, std::optional<response::RoomClosedReason> reason);
    void onRoomExpired(const RoomCode& code);
    void onTurnExpired(const RoomCode& code, std::uint64_t stateVersion);
    void onNextRoundDue(const RoomCode& code, std::uint64_t stateVersion);
    void onGraceExpired(const RoomCode& code, const core::PlayerId& player);
    void onSessionIdle(const core::PlayerId& player);

    MessageSink* sink_;
    RoomRepository* rooms_;
    core::RandomSource* random_;
    const Clock* clock_;
    Scheduler* scheduler_;
    Timeouts timeouts_;

    std::unordered_map<std::string, Session> sessions_; // by player id
    std::unordered_map<std::string, std::string> playerOfToken_;
    std::unordered_map<ConnectionId, std::string> playerOfConnection_;
    std::unordered_map<std::string, std::int64_t> lastReactionAt_; // by player id
    std::vector<std::pair<ConnectionId, response::Message>> outbox_;
};

} // namespace uno::app
