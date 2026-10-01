#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/round.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Small builders shared by the engine tests: they keep test setups short and readable.
namespace uno::testing {

[[nodiscard]] inline core::PlayerId player(std::size_t number)
{
    return core::PlayerId{"player-" + std::to_string(number)};
}

// Players "player-0" .. "player-(count-1)", in seat order.
[[nodiscard]] inline std::vector<core::PlayerId> players(std::size_t count)
{
    std::vector<core::PlayerId> seated;
    seated.reserve(count);
    for (std::size_t number = 0; number < count; ++number) {
        seated.push_back(player(number));
    }
    return seated;
}

[[nodiscard]] inline core::Card coloredCard(std::uint32_t id, core::Color color, core::Rank rank)
{
    return core::Card{.id = core::CardId{id}, .color = color, .rank = rank};
}

[[nodiscard]] inline core::Card wildCard(std::uint32_t id, core::Rank rank)
{
    return core::Card{.id = core::CardId{id}, .color = std::nullopt, .rank = rank};
}

// `count` red Five cards with ids 0, 1, ...: only their ids tell them apart.
[[nodiscard]] inline std::vector<core::Card> plainCards(std::size_t count)
{
    std::vector<core::Card> cards;
    cards.reserve(count);
    for (std::uint32_t id = 0; cards.size() < count; ++id) {
        cards.push_back(coloredCard(id, core::Color::Red, core::Rank::Five));
    }
    return cards;
}

// Starts a round and asserts it succeeded, returning the RoundStart (round + starting events)
// unwrapped. Use this over startedRound() below when a test needs to inspect those events.
[[nodiscard]] inline core::RoundStart startRound(core::RoundSetup setup, core::RandomSource& random)
{
    auto result = core::Round::start(std::move(setup), random);
    REQUIRE(result.has_value());
    return *std::move(result);
}

// Starts a round and asserts it succeeded, returning just the Round.
[[nodiscard]] inline core::Round startedRound(core::RoundSetup setup, core::RandomSource& random)
{
    return startRound(std::move(setup), random).round;
}

// Deals `playerCount` hands of plain cards, then the given cards follow in draw order.
[[nodiscard]] inline std::vector<core::Card> deckFollowingTheHands(std::size_t playerCount,
                                                                   const std::vector<core::Card>& nextCards)
{
    auto deck = plainCards(playerCount * core::kHandSize);
    std::ranges::copy(nextCards, std::back_inserter(deck));
    return deck;
}

// Deck for a round dealt exactly as `hands` says (one 7-card hand per seat, in seat order; the last
// seat deals, so seat 0 plays first). `top` is flipped first, then `afterTop` follows in draw order.
[[nodiscard]] inline std::vector<core::Card> deckFromHands(const std::vector<std::vector<core::Card>>& hands,
                                                           const core::Card& top,
                                                           const std::vector<core::Card>& afterTop = {})
{
    std::vector<core::Card> deck;
    deck.reserve((hands.size() * core::kHandSize) + 1 + afterTop.size());
    for (std::size_t slot = 0; slot < core::kHandSize; ++slot) {
        for (const auto& hand : hands) {
            deck.push_back(hand.at(slot));
        }
    }
    deck.push_back(top);
    std::ranges::copy(afterTop, std::back_inserter(deck));
    return deck;
}

// Same, where the first player holds exactly `firstHand` and every other seat holds red Fives (ids
// from 100).
[[nodiscard]] inline std::vector<core::Card> deckGivingFirstHand(std::size_t playerCount,
                                                                 const std::vector<core::Card>& firstHand,
                                                                 const core::Card& top,
                                                                 const std::vector<core::Card>& afterTop = {})
{
    std::vector<std::vector<core::Card>> hands{firstHand};
    for (std::size_t seat = 1; seat < playerCount; ++seat) {
        auto& hand = hands.emplace_back();
        for (std::size_t slot = 0; slot < core::kHandSize; ++slot) {
            const auto id = 100 + static_cast<std::uint32_t>((seat * core::kHandSize) + slot);
            hand.push_back(coloredCard(id, core::Color::Red, core::Rank::Five));
        }
    }
    return deckFromHands(hands, top, afterTop);
}

[[nodiscard]] inline std::span<const core::Card> handOf(const core::Round& round, const core::PlayerId& player)
{
    const auto hand = round.hand(player);
    REQUIRE(hand.has_value());
    return *hand;
}

[[nodiscard]] inline std::vector<std::uint32_t> idsOf(std::span<const core::Card> cards)
{
    std::vector<std::uint32_t> ids(cards.size());
    std::ranges::transform(cards, ids.begin(), [](const core::Card& card) { return card.id.value; });
    return ids;
}

// Every card of the round, wherever it is, as sorted ids.
[[nodiscard]] inline std::vector<std::uint32_t> allCardIds(const core::Round& round)
{
    auto ids = idsOf(round.drawPile().cards());
    std::ranges::copy(idsOf(round.discardPile().cards()), std::back_inserter(ids));
    for (const auto& seated : round.seats()) {
        std::ranges::copy(idsOf(handOf(round, seated)), std::back_inserter(ids));
    }
    std::ranges::sort(ids);
    return ids;
}

} // namespace uno::testing
