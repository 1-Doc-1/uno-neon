#include "uno/testing/round_invariants.hpp"

#include "uno/core/card.hpp"
#include "uno/core/round.hpp"
#include "uno/core/scoring.hpp"
#include "uno/core/turn_phase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace uno::testing {

namespace {

[[nodiscard]] std::vector<std::uint32_t> allCardIds(const core::Round& round)
{
    std::vector<std::uint32_t> ids;
    const auto collect = [&ids](std::span<const core::Card> cards) {
        for (const auto& card : cards) {
            ids.push_back(card.id.value);
        }
    };
    collect(round.drawPile().cards());
    collect(round.discardPile().cards());
    for (const auto& seated : round.seats()) {
        const auto hand = round.hand(seated);
        REQUIRE(hand.has_value());
        collect(*hand);
    }
    return ids;
}

// No card is ever duplicated across the hands, the draw pile and the discard pile.
void requireNoDuplicateCard(const core::Round& round)
{
    auto ids = allCardIds(round);
    std::ranges::sort(ids);
    REQUIRE(std::ranges::adjacent_find(ids) == ids.end());
}

// The current color is known exactly while play is possible, i.e. everywhere but while the first
// player is still choosing it after a flipped Wild.
void requireCurrentColorMatchesPhase(const core::Round& round)
{
    if (std::holds_alternative<core::AwaitingColorChoice>(round.phase())) {
        REQUIRE(round.currentColor() == std::nullopt);
    } else {
        REQUIRE(round.currentColor().has_value());
    }
}

// While awaiting a drawn-card decision, the card recorded in the phase is still in the current
// player's hand.
void requireDrawnCardStillInHand(const core::Round& round)
{
    const auto* awaitingDrawn = std::get_if<core::AwaitingDrawnCardDecision>(&round.phase());
    if (awaitingDrawn == nullptr) {
        return;
    }
    const auto hand = round.hand(round.currentPlayer());
    REQUIRE(hand.has_value());
    const auto has =
        std::ranges::any_of(*hand, [&](const core::Card& card) { return card.id == awaitingDrawn->drawnCard; });
    REQUIRE(has);
}

// While awaiting a penalty response, the player who played the Wild Draw Four is seated and is not
// the current player: the turn already moved to the one who must respond (SPEC §3).
void requireWildDrawFourPlayerIsNotCurrent(const core::Round& round)
{
    const auto* awaiting = std::get_if<core::AwaitingPenaltyResponse>(&round.phase());
    if (awaiting == nullptr) {
        return;
    }
    REQUIRE(round.hand(awaiting->wildDrawFourPlayer).has_value());
    REQUIRE(round.currentPlayer() != awaiting->wildDrawFourPlayer);
}

// The UNO window only ever concerns a player holding exactly one card who did not announce it.
void requireUnoWindowIsCoherent(const core::Round& round)
{
    const auto& window = round.unoWindow();
    if (!window.has_value()) {
        return;
    }
    const auto hand = round.hand(*window);
    REQUIRE(hand.has_value());
    REQUIRE(hand->size() == 1);
    REQUIRE(!round.hasCalledUno(*window));
}

// An announcement is only kept while the hand it was made for (one or two cards) is unchanged.
void requireAnnouncementsAreCoherent(const core::Round& round)
{
    for (const auto& seated : round.seats()) {
        const auto hand = round.hand(seated);
        REQUIRE(hand.has_value());
        REQUIRE((!round.hasCalledUno(seated) || hand->size() <= 2));
    }
}

// A hand is only ever empty once its owner has won: while the round is in progress everybody holds
// cards. Once over, the winner holds none, the others do, and the points are the value of their cards.
void requireHandMatchesRoundState(const core::Round& round, const core::PlayerId& seated)
{
    const auto* over = std::get_if<core::RoundOver>(&round.phase());
    const auto hand = round.hand(seated);
    REQUIRE(hand.has_value());
    REQUIRE(hand->empty() == (over != nullptr && seated == over->winner));
}

void requireRoundOverIsCoherent(const core::Round& round)
{
    std::uint32_t othersPoints = 0;
    for (const auto& seated : round.seats()) {
        requireHandMatchesRoundState(round, seated);
        othersPoints += core::handPoints(round.hand(seated).value_or(std::span<const core::Card>{}));
    }
    if (const auto* over = std::get_if<core::RoundOver>(&round.phase())) {
        REQUIRE(over->points == othersPoints);
        REQUIRE(round.unoWindow() == std::nullopt);
    }
}

} // namespace

void requireRoundInvariants(const core::Round& round)
{
    requireNoDuplicateCard(round);
    REQUIRE(round.discardPile().size() >= 1);
    requireCurrentColorMatchesPhase(round);
    requireDrawnCardStillInHand(round);
    requireWildDrawFourPlayerIsNotCurrent(round);
    requireRoundOverIsCoherent(round);
    requireUnoWindowIsCoherent(round);
    requireAnnouncementsAreCoherent(round);
}

} // namespace uno::testing
