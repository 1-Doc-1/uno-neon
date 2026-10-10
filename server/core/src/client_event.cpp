#include "uno/core/client_event.hpp"

#include "uno/core/detail/overloaded.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace uno::core {
namespace {

// The cards with these ids, as they are in `player`'s hand right now. Only called for the viewer
// themselves, who may see their own cards.
std::vector<Card> cardsOfHand(const Round& round, const PlayerId& player, std::span<const CardId> ids)
{
    std::vector<Card> cards;
    const auto hand = round.hand(player);
    if (!hand) {
        return cards;
    }
    for (const CardId id : ids) {
        const auto found = std::ranges::find(*hand, id, &Card::id);
        if (found != hand->end()) {
            cards.push_back(*found);
        }
    }
    return cards;
}

CardsDrawnEvent drawnBy(const PlayerId& player, std::span<const CardId> ids, const PlayerId& viewer, const Round& round)
{
    CardsDrawnEvent event{.playerId = player, .count = ids.size(), .cards = std::nullopt};
    if (player == viewer) {
        event.cards = cardsOfHand(round, player, ids);
    }
    return event;
}

} // namespace

std::vector<ClientEvent> project(std::span<const DomainEvent> events, const PlayerId& viewer, const Round& roundAfter,
                                 std::uint32_t roundNumber)
{
    std::vector<ClientEvent> projected;
    projected.reserve(events.size());

    for (const DomainEvent& event : events) {
        std::visit(
            detail::Overloaded{
                [&](const RoundStarted& started) {
                    projected.emplace_back(RoundStartedEvent{.round = roundNumber, .dealerId = started.dealer});
                },
                [&](const CardPlayed& played) {
                    const Card& card = roundAfter.discardPile().top();
                    // The color of a Wild is the current color right after it was played.
                    std::optional<Color> chosen;
                    if (isWild(card.rank)) {
                        chosen = roundAfter.currentColor();
                    }
                    projected.emplace_back(CardPlayedEvent{
                        .playerId = played.player,
                        .card = card,
                        .chosenColor = chosen,
                        .isJumpIn = false,
                    });
                },
                [&](const CardsDrawn& drawn) {
                    projected.emplace_back(drawnBy(drawn.player, drawn.cards, viewer, roundAfter));
                },
                [&](const PenaltyCardsDrawn& drawn) {
                    projected.emplace_back(drawnBy(drawn.player, drawn.cards, viewer, roundAfter));
                },
                [&](const DeckReshuffled& /*reshuffled*/) {
                    projected.emplace_back(DeckReshuffledEvent{.drawPileCount = roundAfter.drawPile().size()});
                },
                [&](const TurnPassed& /*passed*/) {
                    // No animation of its own: the TurnChanged that follows says who plays now.
                },
                [&](const PlayerSkipped& skipped) {
                    projected.emplace_back(PlayerSkippedEvent{.playerId = skipped.skippedPlayer});
                },
                [&](const DirectionReversed& /*reversed*/) {
                    projected.emplace_back(DirectionChangedEvent{.direction = roundAfter.direction()});
                },
                [&](const ColorChosen& chosen) {
                    projected.emplace_back(ColorChosenEvent{.playerId = chosen.player, .color = chosen.color});
                },
                [&](const TurnChanged& changed) {
                    projected.emplace_back(TurnChangedEvent{.playerId = changed.player});
                },
                [&](const ChallengeResolved& resolved) {
                    ChallengeResolvedEvent challenge{
                        .challengerId = resolved.challenger,
                        .challengedId = resolved.challenged,
                        .wasBluff = resolved.wasBluff,
                        .penalizedPlayerId = resolved.penalizedPlayer,
                        .penaltyAmount = resolved.penaltyAmount,
                        .revealedHand = std::nullopt,
                    };
                    // The challenged hand is secret for everyone but the one who challenged (ADR 0007).
                    if (resolved.challenger == viewer) {
                        challenge.revealedHand = resolved.revealedHand;
                    }
                    projected.emplace_back(std::move(challenge));
                },
                [&](const PlusFiveTargeted& targeted) {
                    projected.emplace_back(PlusFiveTargetedEvent{
                        .playerId = targeted.player,
                        .targetId = targeted.target,
                        .total = targeted.total,
                    });
                },
                [&](const UnoCalled& called) { projected.emplace_back(UnoCalledEvent{.playerId = called.player}); },
                [&](const UnoCaught& caught) {
                    projected.emplace_back(UnoCaughtEvent{
                        .catcherId = caught.catcher,
                        .targetId = caught.target,
                        .penaltyAmount = kUnoPenaltyCards,
                    });
                },
                [&](const RoundEnded& ended) {
                    projected.emplace_back(RoundEndedEvent{.winnerId = ended.winner, .points = ended.points});
                },
                [&](const MatchEnded& ended) { projected.emplace_back(MatchEndedEvent{.winnerId = ended.winner}); },
            },
            event);
    }
    return projected;
}

} // namespace uno::core
