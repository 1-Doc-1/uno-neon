#include "uno/testing/round_invariants.hpp"

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"
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

// While a stack of penalties waits for its answer, what is owed is at least what its top card inflicts, and the top
// card is a penalty card. In the official rules only Wild Draw Fives pile up, so the total is a whole number of fives;
// with the ladder they all add up (ADR 0028, 0029).
void requireStackTotalIsCoherent(const core::Round& round)
{
    const auto* awaiting = std::get_if<core::AwaitingStackResponse>(&round.phase());
    if (awaiting == nullptr) {
        return;
    }
    REQUIRE(core::penaltyLevel(awaiting->top).has_value());
    REQUIRE(awaiting->total >= core::penaltyCards(awaiting->top));
    if (round.stacking() == core::PenaltyStacking::Official) {
        REQUIRE(awaiting->top == core::Rank::WildDrawFive);
        REQUIRE(awaiting->total % core::kWildDrawFivePenaltyCards == 0);
    }
}

// A UNO window only ever concerns a player holding exactly one card who did not announce it, once per player.
void requireUnoWindowsAreCoherent(const core::Round& round)
{
    for (const auto& target : round.unoWindows()) {
        const auto hand = round.hand(target);
        REQUIRE(hand.has_value());
        REQUIRE(hand->size() == 1);
        REQUIRE(!round.hasCalledUno(target));
    }
    auto sorted = std::vector<core::PlayerId>{round.unoWindows().begin(), round.unoWindows().end()};
    std::ranges::sort(sorted, {}, &core::PlayerId::value);
    REQUIRE(std::ranges::adjacent_find(sorted) == sorted.end());
}

// An announcement is only kept while the hand it was made for (one or two cards) is unchanged.
void requireAnnouncementsAreCoherent(const core::Round& round)
{
    // One assertion for the whole table: this runs after every action of the massive simulation.
    const auto coherent = std::ranges::all_of(round.seats(), [&](const core::PlayerId& seated) {
        return !round.hasCalledUno(seated) || round.hand(seated).value_or(std::span<const core::Card>{}).size() <= 2;
    });
    REQUIRE(coherent);
}

// A hand is only ever empty once its owner has won: while the round is in progress everybody holds
// cards. Once over, the winner holds none and the others do. The points were the value of every hand when the round
// ended and are already credited; a player who leaves afterwards takes their hand away, so what is left on the table
// can only be worth less (the exact sum is checked at the moment the round ends, by the simulation).
void requireRoundOverIsCoherent(const core::Round& round)
{
    const auto* over = std::get_if<core::RoundOver>(&round.phase());
    std::uint32_t othersPoints = 0;
    bool handsMatchPhase = true;
    for (const auto& seated : round.seats()) {
        const auto hand = round.hand(seated).value_or(std::span<const core::Card>{});
        handsMatchPhase = handsMatchPhase && hand.empty() == (over != nullptr && seated == over->winner);
        othersPoints += core::handPoints(hand);
    }
    REQUIRE(handsMatchPhase);
    if (over != nullptr) {
        REQUIRE(othersPoints <= over->points);
        REQUIRE(round.unoWindows().empty());
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
    requireStackTotalIsCoherent(round);
    requireRoundOverIsCoherent(round);
    requireUnoWindowsAreCoherent(round);
    requireAnnouncementsAreCoherent(round);
}

} // namespace uno::testing
