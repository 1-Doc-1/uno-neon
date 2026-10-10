#include "uno/app/application.hpp"

#include "uno/app/id_generator.hpp"
#include "uno/app/nickname.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/match.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <variant>

namespace uno::app {
namespace {

constexpr int kCloseSessionTakenOver = 4000; // application-defined close code (4000-4999)
constexpr std::int64_t kReactionIntervalMillis = 2000;

} // namespace

Application::Application(MessageSink& sink, RoomRepository& rooms, core::RandomSource& random, const Clock& clock,
                         Scheduler& scheduler, Timeouts timeouts)
    : sink_(&sink), rooms_(&rooms), random_(&random), clock_(&clock), scheduler_(&scheduler), timeouts_(timeouts)
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
    scheduleSessionIdle(session);
    if (!session.room) {
        return;
    }
    if (Room* const room = rooms_->find(*session.room)) {
        if (Member* const member = room->find(session.playerId)) {
            member->connected = false;
            spdlog::info("player {} disconnected from room {}", session.playerId.value, room->code.value);
            startGraceTimer(*room, session.playerId);
            broadcastRoom(*room);
            if (room->phase == response::RoomPhase::InGame) {
                broadcastGame(*room, {}, {core::PlayerDisconnectedEvent{.playerId = session.playerId}});
            }
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
        Session created;
        created.token = generateSessionToken(*random_);
        created.playerId = generatePlayerId(*random_);
        playerOfToken_.emplace(created.token.value, created.playerId.value);
        const std::string playerKey = created.playerId.value;
        session = &sessions_.emplace(playerKey, std::move(created)).first->second;
    }

    session->connection = connection;
    scheduler_->cancel(std::exchange(session->idleTimer, TimerHandle{}));
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
                scheduler_->cancel(std::exchange(room->graceTimers[session->playerId.value], TimerHandle{}));
                spdlog::info("player {} reconnected to room {}", session->playerId.value, room->code.value);
                broadcastRoom(*room);
                if (room->phase == response::RoomPhase::InGame) {
                    // Everyone, the returning player included, gets the state: for them it is the resync.
                    broadcastGame(*room, {}, {core::PlayerReconnectedEvent{.playerId = session->playerId}});
                }
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
    spdlog::info("player {} left room {}", (*session)->playerId.value, (*room)->code.value);
    removeFromRoom(**room, (*session)->playerId);
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
    spdlog::info("player {} kicked from room {}", request.playerId.value, (*room)->code.value);
    removeFromRoom(**room, request.playerId);
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

// ---- matches ----

Application::Outcome Application::handle(ConnectionId connection, const request::StartMatch& /*request*/)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = hostedLobbyOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    if ((*room)->members.size() < core::kMinPlayers) {
        return fail(ErrorCode::NotEnoughPlayers, "At least two players are needed");
    }
    // Starting is the host's own way of saying they are ready.
    const bool everyoneReady = std::ranges::all_of(
        (*room)->members, [&room](const Member& member) { return (*room)->isHost(member.id) || member.ready; });
    if (!everyoneReady) {
        return fail(ErrorCode::PlayersNotReady, "Every player must be ready");
    }
    return startMatch(**room);
}

Application::Outcome Application::startMatch(Room& room)
{
    std::vector<core::PlayerId> seats;
    seats.reserve(room.members.size());
    for (const Member& member : room.members) {
        seats.push_back(member.id);
    }
    auto started = core::Match::start(std::move(seats),
                                      core::MatchSettings{
                                          .matchLength = room.settings.matchLength,
                                          .drawRule = room.settings.drawRule,
                                          .drawAmount = room.settings.drawAmount,
                                          .declareUnoToWin = room.settings.declareUnoToWin,
                                          .deck = room.settings.deck(),
                                      },
                                      *random_);
    if (!started) {
        spdlog::error("room {}: the engine refused to start a match", room.code.value);
        return fail(ErrorCode::InvalidPhase, "The match could not be started");
    }
    room.match = std::move(started->match);
    room.phase = response::RoomPhase::InGame;
    room.readyForNextRound.clear();
    room.formerNicknames.clear();
    spdlog::info("room {}: match started with {} players", room.code.value, room.members.size());
    broadcastRoom(room);
    broadcastGame(room, started->events);
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::Rematch& /*request*/)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    if (!(*room)->isHost((*session)->playerId)) {
        return fail(ErrorCode::NotHost, "Only the host can do this");
    }
    if ((*room)->phase != response::RoomPhase::MatchOver) {
        return fail(ErrorCode::InvalidPhase, "The match is not over");
    }
    // The same players, as long as they are still there (SPEC §5).
    const auto connectedCount =
        std::ranges::count_if((*room)->members, [](const Member& member) { return member.connected; });
    if (std::cmp_less(connectedCount, core::kMinPlayers)) {
        return fail(ErrorCode::NotEnoughPlayers, "At least two connected players are needed");
    }
    std::vector<core::PlayerId> absent;
    for (const Member& member : (*room)->members) {
        if (!member.connected) {
            absent.push_back(member.id);
        }
    }
    for (const core::PlayerId& player : absent) {
        removeFromRoom(**room, player);
    }
    return startMatch(**room);
}

Application::Outcome Application::handle(ConnectionId connection, const request::ReadyForNextRound& /*request*/)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    Room& current = **room;
    const bool betweenRounds = current.phase == response::RoomPhase::InGame && current.match &&
                               std::holds_alternative<core::RoundOver>(current.match->round().phase());
    if (!betweenRounds) {
        return fail(ErrorCode::InvalidPhase, "No round has just ended");
    }

    current.readyForNextRound.insert((*session)->playerId);
    startNextRoundIfDue(current, false);
    return Reply::Ack;
}

Application::Outcome Application::handle(ConnectionId connection, const request::PlayCard& request)
{
    if (request.swapTargetId) {
        return fail(ErrorCode::IllegalMove, "Swapping hands is not available", IllegalMoveReason::SwapTargetInvalid);
    }
    return play(
        connection,
        core::PlayCard{.cardId = request.cardId, .chosenColor = request.chosenColor, .target = request.targetId});
}

Application::Outcome Application::handle(ConnectionId connection, const request::DrawCard& /*request*/)
{
    return play(connection, core::DrawCard{});
}

Application::Outcome Application::handle(ConnectionId connection, const request::Pass& /*request*/)
{
    return play(connection, core::Pass{});
}

Application::Outcome Application::handle(ConnectionId connection, const request::ChooseColor& request)
{
    return play(connection, core::ChooseColor{.color = request.color});
}

Application::Outcome Application::handle(ConnectionId connection, const request::RespondPenalty& request)
{
    return play(connection, core::RespondPenalty{.response = request.response});
}

Application::Outcome Application::handle(ConnectionId connection, const request::CallUno& /*request*/)
{
    return play(connection, core::CallUno{});
}

Application::Outcome Application::handle(ConnectionId connection, const request::CatchUno& request)
{
    // During the grace period the offender is the only one who may still announce (ADR 0018). The engine has no
    // clock, so the application is the one to say "too early".
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    const UnoWindowTiming* const window = (*room)->findUnoWindow(request.targetId);
    if (window != nullptr && (*session)->playerId != request.targetId && clock_->nowMillis() < window->graceEndsAt) {
        return fail(ErrorCode::UnoGracePeriod, "The player may still announce UNO");
    }
    return play(connection, core::CatchUno{.target = request.targetId});
}

Application::Outcome Application::engineFailure(core::DomainError error)
{
    spdlog::error("unexpected engine error {}", static_cast<int>(error));
    return fail(ErrorCode::InvalidPhase, "The engine refused the action");
}

Application::Outcome Application::play(ConnectionId connection, const core::PlayerAction& action)
{
    const auto session = sessionOf(connection);
    if (!session) {
        return std::unexpected(session.error());
    }
    const auto room = roomOf(**session);
    if (!room) {
        return std::unexpected(room.error());
    }
    Room& current = **room;
    if (current.phase != response::RoomPhase::InGame || !current.match) {
        return fail(ErrorCode::InvalidPhase, "No match is running");
    }

    // Nobody acts while the effect in progress is being shown. Announcing UNO and catching are not turn actions.
    // A player who is not on turn is told so by the engine, whatever the time.
    const bool turnAction =
        !std::holds_alternative<core::CallUno>(action) && !std::holds_alternative<core::CatchUno>(action);
    if (turnAction && current.match->round().currentPlayer() == (*session)->playerId &&
        clock_->nowMillis() < current.actionsOpenAt) {
        return fail(ErrorCode::EffectInProgress, "The effect in progress is not over");
    }

    const auto events = current.match->apply((*session)->playerId, action, *random_);
    if (!events) {
        switch (events.error()) {
        case core::DomainError::NotYourTurn:
            return fail(ErrorCode::NotYourTurn, "It is not your turn");
        case core::DomainError::InvalidPhase:
            return fail(ErrorCode::InvalidPhase, "This action is not allowed now");
        case core::DomainError::CardNotInHand:
            return fail(ErrorCode::CardNotInHand, "You do not hold this card");
        case core::DomainError::ColorMismatch:
            return fail(ErrorCode::IllegalMove, "This card cannot be played on the current one",
                        IllegalMoveReason::ColorMismatch);
        case core::DomainError::ColorRequired:
            return fail(ErrorCode::IllegalMove, "A color must be chosen", IllegalMoveReason::ColorRequired);
        case core::DomainError::ColorNotAllowed:
            return fail(ErrorCode::IllegalMove, "A color is only chosen for a Wild",
                        IllegalMoveReason::ColorNotAllowed);
        case core::DomainError::MustPlay:
            return fail(ErrorCode::IllegalMove, "Play a card instead of drawing, or the card you drew",
                        IllegalMoveReason::MustPlay);
        case core::DomainError::MustDeclareUno:
            return fail(ErrorCode::IllegalMove, "Announce UNO before playing your last card",
                        IllegalMoveReason::MustDeclareUno);
        case core::DomainError::OnlyDrawnCardPlayable:
            return fail(ErrorCode::IllegalMove, "Only the card you just drew can be played",
                        IllegalMoveReason::OnlyDrawnCardPlayable);
        case core::DomainError::TargetRequired:
            return fail(ErrorCode::IllegalMove, "A Wild Draw Five needs a target", IllegalMoveReason::TargetRequired);
        case core::DomainError::TargetNotAllowed:
            return fail(ErrorCode::IllegalMove, "Only a Wild Draw Five has a target",
                        IllegalMoveReason::TargetNotAllowed);
        case core::DomainError::InvalidTarget:
            return fail(ErrorCode::IllegalMove, "The target must be another player of the round",
                        IllegalMoveReason::InvalidTarget);
        case core::DomainError::OnlyPlusFivePlayable:
            return fail(ErrorCode::IllegalMove, "Only a Wild Draw Five answers a Wild Draw Five",
                        IllegalMoveReason::OnlyPlusFivePlayable);
        case core::DomainError::CannotChallenge:
            return fail(ErrorCode::IllegalMove, "A Wild Draw Five cannot be challenged",
                        IllegalMoveReason::CannotChallenge);
        case core::DomainError::UnoWindowClosed:
        case core::DomainError::CannotCatchSelf:
        case core::DomainError::UnknownPlayer:
            return fail(ErrorCode::UnoWindowClosed, "Nobody can be caught right now");
        case core::DomainError::NotEnoughPlayers:
        case core::DomainError::TooManyPlayers:
        case core::DomainError::DuplicatePlayer:
        case core::DomainError::DealerNotSeated:
        case core::DomainError::DeckTooSmall:
        case core::DomainError::DuplicateCard:
        case core::DomainError::NoValidStartingCard:
            return engineFailure(events.error());
        }
        return engineFailure(events.error());
    }

    afterMatchChange(current, *events);
    return Reply::Ack;
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
    scheduleRoomExpiry(room);
    ++room.version;
    for (const Member& member : room.members) {
        queue(member.id, response::RoomUpdate{.roomVersion = room.version, .room = room.view()});
    }
}

void Application::broadcastGame(Room& room, std::span<const core::DomainEvent> events,
                                const std::vector<core::ClientEvent>& extraEvents)
{
    if (!room.match.has_value()) {
        return;
    }
    const core::Match& match = *room.match;
    ++room.stateVersion;
    if (std::ranges::any_of(
            events, [](const core::DomainEvent& event) { return std::holds_alternative<core::TurnChanged>(event); })) {
        ++room.turnCount;
    }
    syncUnoWindows(room);
    const std::int64_t serverTime = clock_->nowMillis();
    const auto budget = presentationBudgetOf(events, match.round());
    room.actionsOpenAt = std::max(room.actionsOpenAt, serverTime + budget.count());
    armGameTimers(room);
    for (const Member& member : room.members) {
        auto projected = core::project(events, member.id, match.round(), match.roundNumber());
        projected.insert(projected.end(), extraEvents.begin(), extraEvents.end());
        queue(member.id, response::GameUpdate{
                             .serverTime = serverTime,
                             .events = std::move(projected),
                             .view = viewOf(room, match, member.id),
                         });
    }
}

response::GameView Application::viewOf(const Room& room, const core::Match& match, const core::PlayerId& viewer) const
{
    response::GameView view;
    view.stateVersion = room.stateVersion;
    view.settings = room.settings;
    view.turnDeadline = room.turnDeadline;
    view.nextRoundDeadline = room.nextRoundDeadline;
    view.actionsOpenAt = room.actionsOpenAt;
    view.drawStepMs = static_cast<std::uint32_t>(timeouts_.drawStep.count());
    for (const UnoWindowTiming& window : room.unoWindows) {
        view.unoWindows.push_back(response::UnoWindowInfo{
            .targetId = window.target,
            .graceEndsAt = window.graceEndsAt,
            .expiresAt = window.expiresAt,
        });
    }
    // The viewer is a member of a room whose match is running: the engine knows them.
    if (auto projected = core::project(match, viewer)) {
        view.game = std::move(*projected);
    }
    // One entry per seat of the engine, in seat order. A player who left a match of two leaves a seat behind (the
    // match ended by forfeit): the view still names it.
    for (const core::SeatView& seat : view.game.players) {
        const Member* const member = room.find(seat.playerId);
        const auto former = room.formerNicknames.find(seat.playerId.value);
        std::string nickname;
        if (member != nullptr) {
            nickname = member->nickname;
        } else if (former != room.formerNicknames.end()) {
            nickname = former->second;
        }
        view.seats.push_back(response::SeatInfo{
            .nickname = std::move(nickname),
            .isConnected = member != nullptr && member->connected,
            .isBot = false,
            .isHost = room.isHost(seat.playerId),
            .isReadyForNextRound = room.readyForNextRound.contains(seat.playerId),
        });
    }
    return view;
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
