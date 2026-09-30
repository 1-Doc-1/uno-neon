#pragma once

#include "uno/core/card.hpp"
#include "uno/core/player_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
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

} // namespace uno::testing
