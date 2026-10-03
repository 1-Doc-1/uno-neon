#include "uno/core/player_view.hpp"

#include "uno/core/card.hpp"
#include "uno/core/detail/overloaded.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/match.hpp"
#include "uno/core/playability.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace uno::core {

namespace {

[[nodiscard]] ViewPhase viewPhaseOf(const Round& round, const MatchProgress& progress)
{
    if (progress.winner.has_value()) {
        return ViewPhase::MatchOver;
    }
    return std::visit(detail::Overloaded{
                          [](const AwaitingPlay&) { return ViewPhase::AwaitingPlay; },
                          [](const AwaitingDrawnCardDecision&) { return ViewPhase::AwaitingDrawnCardDecision; },
                          [](const AwaitingColorChoice&) { return ViewPhase::AwaitingColorChoice; },
                          [](const AwaitingPenaltyResponse&) { return ViewPhase::AwaitingPenaltyResponse; },
                          [](const RoundOver&) { return ViewPhase::RoundOver; },
                      },
                      round.phase());
}

// Cards the viewer may play right now: only on their own turn, and only the drawn card after a draw.
[[nodiscard]] std::vector<CardId> playableCardIds(const Round& round, std::span<const Card> hand)
{
    std::vector<CardId> ids;
    if (std::holds_alternative<AwaitingPlay>(round.phase())) {
        for (const auto& card : hand) {
            if (isPlayable(card, round.discardPile().top(), round.currentColor())) {
                ids.push_back(card.id);
            }
        }
    } else if (const auto* drawn = std::get_if<AwaitingDrawnCardDecision>(&round.phase())) {
        ids.push_back(drawn->drawnCard);
    }
    return ids;
}

[[nodiscard]] MyState myStateOf(const Round& round, const PlayerId& viewer, std::span<const Card> hand)
{
    const bool myTurn = round.currentPlayer() == viewer;
    const auto& phase = round.phase();

    MyState me;
    me.playerId = viewer;
    me.hand.assign(hand.begin(), hand.end());
    if (myTurn) {
        me.playableCardIds = playableCardIds(round, hand);
        me.canDraw = std::holds_alternative<AwaitingPlay>(phase);
        me.canPass = std::holds_alternative<AwaitingDrawnCardDecision>(phase);
        me.canChooseColor = std::holds_alternative<AwaitingColorChoice>(phase);
        if (std::holds_alternative<AwaitingPenaltyResponse>(phase)) {
            me.penaltyResponse =
                PenaltyResponseOptions{.amount = kWildDrawFourPenaltyCards, .canChallenge = true, .canStack = false};
        }
    }
    me.canCallUno = round.canCallUno(viewer);
    return me;
}

[[nodiscard]] std::vector<SeatView> seatViewsOf(const Round& round, const MatchProgress& progress)
{
    std::vector<SeatView> views;
    std::size_t seat = 0;
    for (const auto& seated : round.seats()) {
        const auto hand = round.hand(seated).value_or(std::span<const Card>{});
        views.push_back({
            .playerId = seated,
            .seat = seat,
            .cardCount = hand.size(),
            .score = progress.scores.at(seat),
            .hasCalledUno = round.hasCalledUno(seated),
        });
        ++seat;
    }
    return views;
}

// Every remaining hand is public once the round is over (they were counted into the score).
[[nodiscard]] std::optional<RoundResult> roundResultOf(const Round& round)
{
    const auto* over = std::get_if<RoundOver>(&round.phase());
    if (over == nullptr) {
        return std::nullopt;
    }
    RoundResult result{.winnerId = over->winner, .points = over->points, .revealedHands = {}};
    for (const auto& seated : round.seats()) {
        if (const auto hand = round.hand(seated)) {
            result.revealedHands.push_back({.playerId = seated, .cards = {hand->begin(), hand->end()}});
        }
    }
    return result;
}

} // namespace

std::expected<PlayerView, DomainError> project(const Round& round, const PlayerId& viewer,
                                               const MatchProgress& progress)
{
    const auto hand = round.hand(viewer);
    if (!hand) {
        return std::unexpected{hand.error()};
    }

    return PlayerView{
        .phase = viewPhaseOf(round, progress),
        .me = myStateOf(round, viewer, *hand),
        .players = seatViewsOf(round, progress),
        .currentPlayerId = round.currentPlayer(),
        .direction = round.direction(),
        .currentColor = round.currentColor(),
        .discardTop = round.discardPile().top(),
        .drawPileCount = round.drawPile().size(),
        .pendingDraw = std::holds_alternative<AwaitingPenaltyResponse>(round.phase()) ? kWildDrawFourPenaltyCards : 0,
        .round = progress.roundNumber,
        .roundResult = roundResultOf(round),
        .matchWinnerId = progress.winner,
    };
}

std::expected<PlayerView, DomainError> project(const Match& match, const PlayerId& viewer)
{
    return project(match.round(), viewer, match.progress());
}

} // namespace uno::core
