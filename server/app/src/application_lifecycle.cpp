// What happens to rooms, players and matches over time: membership changes, silence (grace periods,
// inactivity, session expiry) and the clocks of a match (turn timer, next round). Part of Application.

#include "uno/app/application.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace uno::app {
namespace {

// Applies an action the server takes for a player whose clock ran out, collecting the events it produces.
void autoApply(core::Match& match, const core::PlayerId& actor, const core::PlayerAction& action,
               core::RandomSource& random, std::vector<core::DomainEvent>& events)
{
    if (auto applied = match.apply(actor, action, random)) {
        events.insert(events.end(), applied->begin(), applied->end());
    }
}

} // namespace

// ---- membership ----

void Application::removeFromRoom(Room& room, const core::PlayerId& player)
{
    const Member* const leaving = room.find(player);
    if (leaving == nullptr) {
        return;
    }
    const std::string nickname = leaving->nickname;
    const bool wasHost = room.isHost(player);
    const bool inMatch = room.phase == response::RoomPhase::InGame && room.match.has_value();

    if (const auto grace = room.graceTimers.find(player.value); grace != room.graceTimers.end()) {
        scheduler_->cancel(grace->second);
        room.graceTimers.erase(grace);
    }
    if (const auto session = sessions_.find(player.value); session != sessions_.end()) {
        session->second.room.reset();
        if (!session->second.connection) {
            scheduleSessionIdle(session->second);
        }
    }

    std::vector<core::DomainEvent> events;
    if (inMatch) {
        auto removed = room.match->removePlayer(player, *random_);
        if (removed) {
            events = std::move(*removed);
        } else {
            spdlog::error("room {}: the engine refused to remove player {}", room.code.value, player.value);
        }
        room.formerNicknames.insert_or_assign(player.value, nickname);
    }
    room.remove(player);
    room.readyForNextRound.erase(player);

    if (room.members.empty()) {
        spdlog::info("room {} closed: empty", room.code.value);
        destroyRoom(room, std::nullopt);
        return;
    }
    broadcastRoom(room);
    if (inMatch) {
        std::vector<core::ClientEvent> extra;
        if (wasHost) {
            extra.emplace_back(core::HostChangedEvent{.playerId = room.host});
        }
        afterMatchChange(room, events, extra);
        // The player who left may have been the only one the next round was waiting for.
        startNextRoundIfDue(room, false);
    }
}

void Application::afterMatchChange(Room& room, std::span<const core::DomainEvent> events,
                                   const std::vector<core::ClientEvent>& extraEvents)
{
    const bool over = room.match.has_value() && room.match->winner().has_value();
    if (over && room.phase != response::RoomPhase::MatchOver) {
        room.phase = response::RoomPhase::MatchOver;
        spdlog::info("room {}: match over", room.code.value);
    }
    broadcastGame(room, events, extraEvents);
    if (over) {
        broadcastRoom(room);
    }
}

void Application::startNextRoundIfDue(Room& room, bool force)
{
    const bool betweenRounds = room.phase == response::RoomPhase::InGame && room.match.has_value() &&
                               !room.match->winner().has_value() &&
                               std::holds_alternative<core::RoundOver>(room.match->round().phase());
    if (!betweenRounds) {
        return;
    }
    const bool everyoneReady = force || std::ranges::all_of(room.members, [&room](const Member& member) {
                                   return !member.connected || room.readyForNextRound.contains(member.id);
                               });
    std::vector<core::DomainEvent> events;
    if (everyoneReady) {
        auto next = room.match->startNextRound(*random_);
        if (!next) {
            spdlog::error("room {}: the engine refused to deal the next round", room.code.value);
            return;
        }
        events = std::move(*next);
        room.readyForNextRound.clear();
    }
    // Even when somebody is still missing, everyone is told who is ready.
    broadcastGame(room, events);
}

// ---- timers ----

void Application::scheduleRoomExpiry(Room& room)
{
    scheduler_->cancel(std::exchange(room.expiryTimer, TimerHandle{}));
    std::chrono::milliseconds delay{};
    switch (room.phase) {
    case response::RoomPhase::Lobby:
        delay = timeouts_.lobbyInactivity;
        break;
    case response::RoomPhase::MatchOver:
        delay = timeouts_.matchOverInactivity;
        break;
    case response::RoomPhase::InGame:
        return; // a match that is running is kept alive by its players, and by the grace periods
    }
    room.expiryTimer = scheduler_->schedule(delay, [this, code = room.code] { onRoomExpired(code); });
}

void Application::armGameTimers(Room& room)
{
    scheduler_->cancel(std::exchange(room.turnTimer, TimerHandle{}));
    scheduler_->cancel(std::exchange(room.nextRoundTimer, TimerHandle{}));
    room.turnDeadline.reset();
    room.nextRoundDeadline.reset();
    if (room.phase != response::RoomPhase::InGame || !room.match.has_value() || room.match->winner().has_value()) {
        return;
    }
    const std::int64_t now = clock_->nowMillis();
    const std::uint64_t version = room.stateVersion;
    if (std::holds_alternative<core::RoundOver>(room.match->round().phase())) {
        room.nextRoundDeadline = now + timeouts_.nextRound.count();
        room.nextRoundTimer = scheduler_->schedule(
            timeouts_.nextRound, [this, code = room.code, version] { onNextRoundDue(code, version); });
        return;
    }
    const auto turnTime = std::chrono::seconds(static_cast<int>(room.settings.turnTimer));
    if (turnTime.count() == 0) {
        return;
    }
    room.turnDeadline = now + std::chrono::duration_cast<std::chrono::milliseconds>(turnTime).count();
    room.turnTimer =
        scheduler_->schedule(turnTime, [this, code = room.code, version] { onTurnExpired(code, version); });
}

void Application::syncUnoWindows(Room& room)
{
    const auto open = room.match.has_value() ? room.match->round().unoWindows() : std::span<const core::PlayerId>{};
    for (auto timing = room.unoWindows.begin(); timing != room.unoWindows.end();) {
        if (std::ranges::find(open, timing->target) == open.end()) {
            scheduler_->cancel(timing->expiryTimer);
            timing = room.unoWindows.erase(timing);
        } else {
            ++timing;
        }
    }
    const std::int64_t now = clock_->nowMillis();
    for (const core::PlayerId& target : open) {
        if (room.findUnoWindow(target) != nullptr) {
            continue;
        }
        const std::int64_t expiresAt = now + timeouts_.unoWindow.count();
        room.unoWindows.push_back(UnoWindowTiming{
            .target = target,
            .graceEndsAt = now + timeouts_.unoGrace.count(),
            .expiresAt = expiresAt,
            .expiryTimer = scheduler_->schedule(
                timeouts_.unoWindow,
                // NOLINTNEXTLINE(bugprone-exception-escape): only out-of-memory
                [this, code = room.code, target, expiresAt] { onUnoWindowExpired(code, target, expiresAt); }),
        });
    }
}

void Application::onUnoWindowExpired(const RoomCode& code, const core::PlayerId& target, std::int64_t expiresAt)
{
    Room* const room = rooms_->find(code);
    const UnoWindowTiming* const window = room != nullptr ? room->findUnoWindow(target) : nullptr;
    // A window closed and opened again since is another window, with its own timer.
    if (window == nullptr || window->expiresAt != expiresAt || !room->match.has_value()) {
        return;
    }
    if (room->match->closeUnoWindow(target)) {
        afterMatchChange(*room, {});
    }
    flush();
}

void Application::startGraceTimer(Room& room, const core::PlayerId& player)
{
    TimerHandle& timer = room.graceTimers[player.value];
    scheduler_->cancel(std::exchange(timer, TimerHandle{}));
    timer = scheduler_->schedule(
        timeouts_.reconnectGrace,
        // NOLINTNEXTLINE(bugprone-exception-escape): copying the captures can only fail by running out of memory
        [this, code = room.code, player] { onGraceExpired(code, player); });
}

void Application::scheduleSessionIdle(Session& session)
{
    scheduler_->cancel(std::exchange(session.idleTimer, TimerHandle{}));
    session.idleTimer =
        scheduler_->schedule(timeouts_.sessionIdle, [this, player = session.playerId] { onSessionIdle(player); });
}

void Application::destroyRoom(Room& room, std::optional<response::RoomClosedReason> reason)
{
    const RoomCode code = room.code;
    scheduler_->cancel(std::exchange(room.expiryTimer, TimerHandle{}));
    scheduler_->cancel(std::exchange(room.turnTimer, TimerHandle{}));
    scheduler_->cancel(std::exchange(room.nextRoundTimer, TimerHandle{}));
    for (const UnoWindowTiming& window : room.unoWindows) {
        scheduler_->cancel(window.expiryTimer);
    }
    for (const auto& [player, timer] : room.graceTimers) {
        scheduler_->cancel(timer);
    }
    for (const Member& member : room.members) {
        if (reason) {
            queue(member.id, response::RoomClosed{*reason});
        }
        if (const auto session = sessions_.find(member.id.value); session != sessions_.end()) {
            session->second.room.reset();
            if (!session->second.connection) {
                scheduleSessionIdle(session->second);
            }
        }
    }
    rooms_->remove(code); // `room` is gone
}

void Application::onRoomExpired(const RoomCode& code)
{
    if (Room* const room = rooms_->find(code)) {
        spdlog::info("room {} closed: inactive", code.value);
        destroyRoom(*room, response::RoomClosedReason::Expired);
    }
    flush();
}

void Application::onTurnExpired(const RoomCode& code, std::uint64_t stateVersion)
{
    Room* const room = rooms_->find(code);
    // A timer that outlived the state it was armed for does nothing: every change arms a new one.
    if (room == nullptr || room->stateVersion != stateVersion || !room->match.has_value() ||
        room->phase != response::RoomPhase::InGame || room->match->winner().has_value()) {
        return;
    }
    core::Match& match = *room->match;
    const core::PlayerId actor = match.round().currentPlayer();
    std::vector<core::DomainEvent> events;
    // Every automatic action goes through Match::apply, like a player's own: the same rules, the same events,
    // and the UNO window closes as it would have.

    const core::TurnPhase& phase = match.round().phase();
    if (std::holds_alternative<core::AwaitingPenaltyResponse>(phase)) {
        autoApply(match, actor, core::RespondPenalty{.response = core::PenaltyResponse::Accept}, *random_, events);
    } else if (std::holds_alternative<core::AwaitingColorChoice>(phase)) {
        const auto color = core::kColors.at(random_->uniform(static_cast<std::uint32_t>(core::kColors.size())));
        autoApply(match, actor, core::ChooseColor{.color = color}, *random_, events);
    } else if (std::holds_alternative<core::AwaitingDrawnCardDecision>(phase)) {
        autoApply(match, actor, core::Pass{}, *random_, events);
    } else if (std::holds_alternative<core::AwaitingPlay>(phase)) {
        autoApply(match, actor, core::DrawCard{}, *random_, events);
        // The card drawn may be playable, but a player who let the clock run out does not play it.
        if (std::holds_alternative<core::AwaitingDrawnCardDecision>(match.round().phase())) {
            autoApply(match, actor, core::Pass{}, *random_, events);
        }
    }
    spdlog::info("room {}: turn of {} timed out", code.value, actor.value);
    afterMatchChange(*room, events);
    flush();
}

void Application::onNextRoundDue(const RoomCode& code, std::uint64_t stateVersion)
{
    Room* const room = rooms_->find(code);
    if (room != nullptr && room->stateVersion == stateVersion) {
        startNextRoundIfDue(*room, true);
    }
    flush();
}

void Application::onGraceExpired(const RoomCode& code, const core::PlayerId& player)
{
    Room* const room = rooms_->find(code);
    if (room == nullptr) {
        return;
    }
    room->graceTimers.erase(player.value);
    const Member* const member = room->find(player);
    if (member != nullptr && !member->connected) {
        spdlog::info("player {} did not come back to room {}: removed", player.value, code.value);
        removeFromRoom(*room, player);
    }
    flush();
}

void Application::onSessionIdle(const core::PlayerId& player)
{
    const auto found = sessions_.find(player.value);
    if (found == sessions_.end() || found->second.connection.has_value()) {
        return;
    }
    if (found->second.room) {
        if (Room* const room = rooms_->find(*found->second.room)) {
            removeFromRoom(*room, player);
        }
    }
    playerOfToken_.erase(found->second.token.value);
    lastReactionAt_.erase(player.value);
    sessions_.erase(player.value);
    flush();
}

} // namespace uno::app
