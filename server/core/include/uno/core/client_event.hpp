#pragma once

#include "uno/core/card.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace uno::core {

// What a client is told to animate (protocol/schema/client-event.schema.json), projected from the
// DomainEvents for ONE viewer. Like PlayerView, it is the only form in which events ever leave the
// engine: a field that only some players may know is optional and filled for them alone.

struct RoundStartedEvent {
    std::uint32_t round{};
    PlayerId dealerId;

    bool operator==(const RoundStartedEvent&) const = default;
};

struct CardPlayedEvent {
    PlayerId playerId;
    Card card; // public from the moment it is played
    std::optional<Color> chosenColor;
    bool isJumpIn{};

    bool operator==(const CardPlayedEvent&) const = default;
};

// `cards` is present ONLY for the player who drew: everyone else sees how many.
struct CardsDrawnEvent {
    PlayerId playerId;
    std::size_t count{};
    std::optional<std::vector<Card>> cards;

    bool operator==(const CardsDrawnEvent&) const = default;
};

struct TurnChangedEvent {
    PlayerId playerId;

    bool operator==(const TurnChangedEvent&) const = default;
};

struct PlayerSkippedEvent {
    PlayerId playerId;

    bool operator==(const PlayerSkippedEvent&) const = default;
};

struct DirectionChangedEvent {
    Direction direction{};

    bool operator==(const DirectionChangedEvent&) const = default;
};

struct ColorChosenEvent {
    PlayerId playerId;
    Color color{};

    bool operator==(const ColorChosenEvent&) const = default;
};

struct PenaltyStackedEvent {
    PlayerId playerId;
    std::size_t pendingDraw{};

    bool operator==(const PenaltyStackedEvent&) const = default;
};

// `revealedHand` (the challenged player's hand when they played) is present ONLY for the challenger.
struct ChallengeResolvedEvent {
    PlayerId challengerId;
    PlayerId challengedId;
    bool wasBluff{};
    PlayerId penalizedPlayerId;
    std::size_t penaltyAmount{};
    std::optional<std::vector<Card>> revealedHand;

    bool operator==(const ChallengeResolvedEvent&) const = default;
};

// A Wild Draw Five was played or answered (ADR 0028): `targetId` must draw `total` cards or answer with another one.
struct PlusFiveTargetedEvent {
    PlayerId playerId; // who played the Wild Draw Five
    PlayerId targetId;
    std::size_t total{};

    bool operator==(const PlusFiveTargetedEvent&) const = default;
};

struct UnoCalledEvent {
    PlayerId playerId;

    bool operator==(const UnoCalledEvent&) const = default;
};

struct UnoCaughtEvent {
    PlayerId catcherId;
    PlayerId targetId;
    std::size_t penaltyAmount{};

    bool operator==(const UnoCaughtEvent&) const = default;
};

struct HandsSwappedEvent {
    PlayerId playerId;
    PlayerId targetId;

    bool operator==(const HandsSwappedEvent&) const = default;
};

struct HandsRotatedEvent {
    Direction direction{};

    bool operator==(const HandsRotatedEvent&) const = default;
};

struct DeckReshuffledEvent {
    std::size_t drawPileCount{};

    bool operator==(const DeckReshuffledEvent&) const = default;
};

struct RoundEndedEvent {
    PlayerId winnerId;
    std::uint32_t points{};

    bool operator==(const RoundEndedEvent&) const = default;
};

struct MatchEndedEvent {
    PlayerId winnerId;

    bool operator==(const MatchEndedEvent&) const = default;
};

// Not produced by the engine, which knows nothing of connections: the application adds them.
struct PlayerDisconnectedEvent {
    PlayerId playerId;

    bool operator==(const PlayerDisconnectedEvent&) const = default;
};

struct PlayerReconnectedEvent {
    PlayerId playerId;

    bool operator==(const PlayerReconnectedEvent&) const = default;
};

struct HostChangedEvent {
    PlayerId playerId;

    bool operator==(const HostChangedEvent&) const = default;
};

using ClientEvent = std::variant<RoundStartedEvent, CardPlayedEvent, CardsDrawnEvent, TurnChangedEvent,
                                 PlayerSkippedEvent, DirectionChangedEvent, ColorChosenEvent, PenaltyStackedEvent,
                                 ChallengeResolvedEvent, PlusFiveTargetedEvent, UnoCalledEvent, UnoCaughtEvent,
                                 HandsSwappedEvent, HandsRotatedEvent, DeckReshuffledEvent, RoundEndedEvent,
                                 MatchEndedEvent, PlayerDisconnectedEvent, PlayerReconnectedEvent, HostChangedEvent>;

// Projects the events a Round or Match just produced for `viewer` (SPEC §7.2; ADR 0015). `roundAfter`
// is the round as it stands right after those events, which is how the card of a CardPlayed (the top
// of the discard pile), the cards a player drew (in their hand) and the direction of a Reverse are
// found: one action plays at most one card and reverses at most once, so the state after is the state
// at the time. `roundNumber` is the number of the round the events belong to.
[[nodiscard]] std::vector<ClientEvent> project(std::span<const DomainEvent> events, const PlayerId& viewer,
                                               const Round& roundAfter, std::uint32_t roundNumber);

} // namespace uno::core
