#pragma once

// Security invariant 2 on the wire (CLAUDE.md): what the server sends a player, serialized as JSON, shows no card
// they may not see. Shared by the tests of the application and of the whole server.

#include "uno/app/server_message.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/player_id.hpp"
#include "uno/net/codec.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <variant>
#include <vector>

namespace uno::testing {

// Every card id the JSON of a message shows: every object that has the three fields of a card, plus the
// ids listed in playableCardIds.
inline void collectCardIds(const nlohmann::json& json, std::set<std::int64_t>& ids)
{
    if (json.is_object()) {
        if (json.contains("id") && json.contains("color") && json.contains("rank")) {
            ids.insert(json.at("id").get<std::int64_t>());
        }
        if (json.contains("playableCardIds")) {
            for (const auto& id : json.at("playableCardIds")) {
                ids.insert(id.get<std::int64_t>());
            }
        }
        for (const auto& [key, value] : json.items()) {
            collectCardIds(value, ids);
        }
    } else if (json.is_array()) {
        for (const auto& element : json) {
            collectCardIds(element, ids);
        }
    }
}

// Security invariant 2 on the wire: whatever the server sent `viewer`, serialized, shows no card they are
// not entitled to see: their hand, the top of the pile, the hands of a finished round, a hand revealed to
// them by their own challenge.
inline void requireWireLeaksNothing(const app::response::GameUpdate& update, const core::PlayerId& viewer)
{
    const auto json = nlohmann::json::parse(uno::net::encodeServerMessage(update));
    std::set<std::int64_t> shown;
    collectCardIds(json, shown);

    std::set<std::int64_t> entitled;
    for (const auto& card : update.view.game.me.hand) {
        entitled.insert(card.id.value);
    }
    entitled.insert(update.view.game.discardTop.id.value);
    if (update.view.game.roundResult) {
        for (const auto& revealed : update.view.game.roundResult->revealedHands) {
            for (const auto& card : revealed.cards) {
                entitled.insert(card.id.value);
            }
        }
    }
    for (const auto& event : update.events) {
        if (const auto* challenge = std::get_if<core::ChallengeResolvedEvent>(&event)) {
            REQUIRE((challenge->challengerId == viewer) == challenge->revealedHand.has_value());
            for (const auto& card : challenge->revealedHand.value_or(std::vector<core::Card>{})) {
                entitled.insert(card.id.value);
            }
        }
        if (const auto* drawn = std::get_if<core::CardsDrawnEvent>(&event)) {
            REQUIRE((drawn->playerId == viewer) == drawn->cards.has_value());
        }
    }
    const bool onlyEntitled = std::ranges::all_of(shown, [&](std::int64_t id) { return entitled.contains(id); });
    REQUIRE(onlyEntitled);
}

} // namespace uno::testing
