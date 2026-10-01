#include "uno/testing/view_leak_check.hpp"

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <variant>
#include <vector>

namespace uno::testing {

namespace {

[[nodiscard]] std::vector<core::CardId> idsOf(std::span<const core::Card> cards)
{
    std::vector<core::CardId> ids;
    for (const auto& card : cards) {
        ids.push_back(card.id);
    }
    return ids;
}

// Every card id the view mentions, wherever it appears.
[[nodiscard]] std::vector<core::CardId> exposedIds(const core::PlayerView& view)
{
    auto ids = idsOf(view.me.hand);
    ids.insert(ids.end(), view.me.playableCardIds.begin(), view.me.playableCardIds.end());
    ids.push_back(view.discardTop.id);
    if (view.roundResult.has_value()) {
        for (const auto& revealed : view.roundResult->revealedHands) {
            const auto revealedIds = idsOf(revealed.cards);
            ids.insert(ids.end(), revealedIds.begin(), revealedIds.end());
        }
    }
    return ids;
}

// Cards the viewer is entitled to see: their hand, the discard top, and every hand once the round is over.
[[nodiscard]] std::vector<core::CardId> visibleIds(const core::Round& round, const core::PlayerId& viewer)
{
    auto ids = idsOf(round.hand(viewer).value_or(std::span<const core::Card>{}));
    ids.push_back(round.discardPile().top().id);
    if (std::holds_alternative<core::RoundOver>(round.phase())) {
        for (const auto& seated : round.seats()) {
            const auto handIds = idsOf(round.hand(seated).value_or(std::span<const core::Card>{}));
            ids.insert(ids.end(), handIds.begin(), handIds.end());
        }
    }
    return ids;
}

void requireOnlyEntitledCardsAreExposed(const core::Round& round, const core::PlayerView& view,
                                        const core::PlayerId& viewer)
{
    const auto entitled = visibleIds(round, viewer);
    const auto exposed = exposedIds(view);
    const auto onlyEntitled = std::ranges::all_of(
        exposed, [&](const core::CardId& id) { return std::ranges::find(entitled, id) != entitled.end(); });
    REQUIRE(onlyEntitled);
    REQUIRE((view.roundResult.has_value() == std::holds_alternative<core::RoundOver>(round.phase())));
}

void requireCountsMatchWithoutShowingCards(const core::Round& round, const core::PlayerView& view)
{
    REQUIRE(view.drawPileCount == round.drawPile().size());
    REQUIRE(view.players.size() == round.seats().size());
    const auto countsMatch = std::ranges::all_of(view.players, [&](const core::SeatView& seat) {
        return seat.cardCount == round.hand(seat.playerId).value_or(std::span<const core::Card>{}).size();
    });
    REQUIRE(countsMatch);
}

} // namespace

void requireViewLeaksNothing(const core::Round& round, const core::PlayerView& view, const core::PlayerId& viewer)
{
    requireOnlyEntitledCardsAreExposed(round, view, viewer);
    requireCountsMatchWithoutShowingCards(round, view);
}

} // namespace uno::testing
