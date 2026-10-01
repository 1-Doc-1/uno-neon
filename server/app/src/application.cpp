#include "uno/app/application.hpp"

#include "uno/app/id_generator.hpp"
#include "uno/app/nickname.hpp"
#include "uno/app/room_settings.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>
#include <variant>

namespace uno::app {
namespace {

constexpr int kCloseSessionTakenOver = 4000; // application-defined close code (4000-4999)
constexpr std::int64_t kReactionIntervalMillis = 2000;

} // namespace

Application::Application(MessageSink& sink, RoomRepository& rooms, core::RandomSource& random, const Clock& clock)
    : sink_(&sink), rooms_(&rooms), random_(&random), clock_(&clock)
{
}

std::unexpected<Application::Failure> Application::fail(ErrorCode code, std::string message,
                                                        std::optional<IllegalMoveReason> reason)
{
    return std::unexpected(Failure{.code = code, .message = std::move(message), .reason = reason});
}

void Application::onConnected(ConnectionId connection)
{
    spdlog::debug("connection {} opened", connection.value);
}

void Application::onRequest(ConnectionId connection, request::Envelope request)
{
    const Outcome outcome =
        std::visit([this, connection](const auto& body) { return handle(connection, body); }, request.body);
    if (outcome) {
        if (*outcome == Reply::Ack) {
            sink_->send(connection, response::Ack{std::move(request.id)});
        }
    } else {
        // A failed request changes nothing and says nothing to anyone else.
        outbox_.clear();
        sink_->send(connection, response::Error{
                                    .replyTo = std::move(request.id),
                                    .code = outcome.error().code,
                                    .message = outcome.error().message,
                                    .reason = outcome.error().reason,
                                });
    }
    flush();
}

void Application::onDisconnected(ConnectionId connection)
{
    spdlog::debug("connection {} closed", connection.value);
    const auto player = playerOfConnection_.find(connection);
    if (player == playerOfConnection_.end()) {
        return;
    }
    Session& session = sessions_.at(player->second);
    playerOfConnection_.erase(player);
    session.connection.reset();
    if (!session.room) {
        return;
    }
    if (Room* const room = rooms_->find(*session.room)) {
        if (Member* const member = room->find(session.playerId)) {
            member->connected = false;
            spdlog::info("player {} disconnected from room {}", session.playerId.value, room->code.value);
            broadcastRoom(*room);
        }
    }
    flush();
}

// ---- sessions ----

Application::Outcome Application::handle(ConnectionId connection, const request::Hello& request)
{
    if (playerOfConnection_.contains(connection)) {
        return fail(ErrorCode::InvalidPhase, "The session is already established on this connection");
    }

    Session* session = nullptr;
    if (request.sessionToken) {
        const auto player = playerOfToken_.find(request.sessionToken->value);
        if (player == playerOfToken_.end()) {
            return fail(ErrorCode::SessionExpired, "Unknown or expired session token");
        }
        session = &sessions_.at(player->second);
        if (session->connection) {
            // The same player opened a second tab or reconnected before the old socket was noticed dead:
            // the newest connection wins. Forgetting the old one first keeps its late close harmless.
            const ConnectionId previous = *session->connection;
            playerOfConnection_.erase(previous);
            sink_->close(previous, kCloseSessionTakenOver, "Session resumed on another connection");
        }
    } else {
        Session created{
            .token = generateSessionToken(*random_),
            .playerId = generatePlayerId(*random_),
            .connection = std::nullopt,
            .room = std::nullopt,
        };
        playerOfToken_.emplace(created.token.value, created.playerId.value);
        const std::string playerKey = created.playerId.value;
        session = &sessions_.emplace(playerKey, std::move(created)).first->second;
    }

    session->connection = connection;
    playerOfConnection_.emplace(connection, session->playerId.value);
    queue(session->playerId, response::Welcome{
                                 .sessionToken = session->token,
                                 .playerId = session->playerId,
                                 .resumedRoomCode = session->room,
                             });

    if (request.sessionToken && session->room) {
        if (Room* const room = rooms_->find(*session->room)) {
            if (Member* const member = room->find(session->playerId)) {
                member->connected = true;
                spdlog::info("player {} reconnected to room {}", session->playerId.value, room->code.value);
                broadcastRoom(*room);
            }
        }
    }
    return Reply::None;
}

std::expected<Application::Session*, Application::Failure> Application::sessionOf(ConnectionId connection)
{
    const auto player = playerOfConnection_.find(connection);
    if (player == playerOfConnection_.end()) {
        return fail(ErrorCode::SessionRequired, "Send session.hello first");
    }
    return &sessions_.at(player->second);
}

std::expected<Room*, Application::Failure> Application::roomOf(const Session& session)
{
    if (session.room) {
        if (Room* const room = rooms_->find(*session.room)) {
            return room;
        }
    }
    return fail(ErrorCode::NotInRoom, "You are not in a room");
}

std::expected<Room*, Application::Failure> Application::hostedLobbyOf(const Session& session)
{
    const auto room = roomOf(session);
    if (!room) {
        return room;
    }
    if (!(*room)->isHost(session.playerId)) {
        return fail(ErrorCode::NotHost, "Only the host can do this");
    }
    if ((*room)->phase != response::RoomPhase::Lobby) {
        return fail(ErrorCode::MatchInProgress, "The match has already started");
    }
    return room;
}

// ---- rooms ----

Application::Outcome Application::handle(ConnectionId connection, const request::CreateRoom& request)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    if ((*session)->room) {
        return fail(ErrorCode::AlreadyInRoom, "Leave your current room first");
    }
    const auto nickname = validateNickname(request.nickname);
    if (!nickname) {
        return fail(ErrorCode::NicknameInvalid, "A nickname has 2 to 16 letters, digits, spaces, _ or -");
    }
    RoomSettings settings;
    if (request.settings) {
        settings = applyPatch(settings, *request.settings);
    }
    if (const auto problem = settingsProblem(settings, 1)) {
        return fail(ErrorCode::InvalidSettings, *problem);
    }

    const RoomCode code =
        generateRoomCode(*random_, [this](const RoomCode& candidate) { return rooms_->find(candidate) != nullptr; });
    Room& room = rooms_->add(Room(code, settings, (*session)->playerId, *nickname));
    (*session)->room = code;
    spdlog::info("room {} created by player {}", code.value, (*session)->playerId.value);
    broadcastRoom(room);
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::JoinRoom& request)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    if ((*session)->room) {
        return fail(ErrorCode::AlreadyInRoom, "Leave your current room first");
    }
    const auto nickname = validateNickname(request.nickname);
    if (!nickname) {
        return fail(ErrorCode::NicknameInvalid, "A nickname has 2 to 16 letters, digits, spaces, _ or -");
    }
    Room* const room = rooms_->find(request.code);
    if (room == nullptr) {
        return fail(ErrorCode::RoomNotFound, "No room has this code");
    }
    if (room->phase != response::RoomPhase::Lobby) {
        return fail(ErrorCode::MatchInProgress, "The match has already started");
    }
    if (room->isFull()) {
        return fail(ErrorCode::RoomFull, "The room is full");
    }
    if (room->nicknameTaken(*nickname)) {
        return fail(ErrorCode::NicknameTaken, "Another player already uses this nickname");
    }

    room->add((*session)->playerId, *nickname);
    (*session)->room = room->code;
    spdlog::info("player {} joined room {}", (*session)->playerId.value, room->code.value);
    broadcastRoom(*room);
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::LeaveRoom& /*request*/)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    // Leaving a running match needs the engine to take a player out of a round: not before step 2.5.
    if ((*room)->phase == response::RoomPhase::InGame) {
        return fail(ErrorCode::MatchInProgress, "You cannot leave while a match is running");
    }

    (*room)->remove((*session)->playerId);
    (*session)->room.reset();
    spdlog::info("player {} left room {}", (*session)->playerId.value, (*room)->code.value);
    if ((*room)->members.empty()) {
        spdlog::info("room {} closed: empty", (*room)->code.value);
        rooms_->remove((*room)->code);
    } else {
        broadcastRoom(**room);
    }
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::UpdateSettings& request)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = hostedLobbyOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    const RoomSettings updated = applyPatch((*room)->settings, request.settings);
    if (const auto problem = settingsProblem(updated, (*room)->members.size())) {
        return fail(ErrorCode::InvalidSettings, *problem);
    }
    (*room)->settings = updated;
    broadcastRoom(**room);
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::SetReady& request)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    if ((*room)->phase != response::RoomPhase::Lobby) {
        return fail(ErrorCode::MatchInProgress, "The match has already started");
    }
    if (Member* const member = (*room)->find((*session)->playerId)) {
        member->ready = request.ready;
    }
    broadcastRoom(**room);
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::Kick& request)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = hostedLobbyOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    if (request.playerId == (*session)->playerId) {
        return fail(ErrorCode::CannotKickSelf, "The host cannot kick themselves");
    }
    if ((*room)->find(request.playerId) == nullptr) {
        return fail(ErrorCode::NotInRoom, "This player is not in the room");
    }

    queue(request.playerId, response::RoomClosed{response::RoomClosedReason::Kicked});
    (*room)->remove(request.playerId);
    if (const auto kicked = sessions_.find(request.playerId.value); kicked != sessions_.end()) {
        kicked->second.room.reset();
    }
    spdlog::info("player {} kicked from room {}", request.playerId.value, (*room)->code.value);
    broadcastRoom(**room);
    return Reply::Ack;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): same signature as the other handlers
Application::Outcome Application::handle(ConnectionId /*connection*/, const request::AddBot& /*request*/)
{
    return fail(ErrorCode::UnknownType, "Bots are not available yet");
}

Application::Outcome Application::handle(ConnectionId connection, const request::SendReaction& request)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    const std::int64_t now = clock_->nowMillis();
    const auto previous = lastReactionAt_.find((*session)->playerId.value);
    if (previous != lastReactionAt_.end() && now - previous->second < kReactionIntervalMillis) {
        return fail(ErrorCode::RateLimited, "One reaction every 2 seconds");
    }
    lastReactionAt_.insert_or_assign((*session)->playerId.value, now);
    for (const Member& member : (*room)->members) {
        queue(member.id, response::Reaction{.playerId = (*session)->playerId, .emote = request.emote});
    }
    return Reply::Ack;
}

// Placeholder until step 2.4 gives each of these requests its handler.
template <typename Request>
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Application::Outcome Application::handle(ConnectionId /*connection*/, const Request& /*request*/)
{
    return fail(ErrorCode::InvalidPhase, "Matches are not available yet");
}

// ---- output ----

void Application::queue(const core::PlayerId& player, response::Message message)
{
    const auto session = sessions_.find(player.value);
    if (session == sessions_.end()) {
        return;
    }
    if (const auto& connection = session->second.connection) {
        outbox_.emplace_back(*connection, std::move(message));
    }
}

void Application::broadcastRoom(Room& room)
{
    ++room.version;
    for (const Member& member : room.members) {
        queue(member.id, response::RoomUpdate{.roomVersion = room.version, .room = room.view()});
    }
}

void Application::flush()
{
    // Sending cannot re-enter the application, but swap anyway: the queue is empty whatever happens next.
    const auto pending = std::exchange(outbox_, {});
    for (const auto& [connection, message] : pending) {
        sink_->send(connection, message);
    }
}

} // namespace uno::app
