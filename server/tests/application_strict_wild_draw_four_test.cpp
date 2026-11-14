#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/room_settings.hpp"
#include "uno/core/card.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/core/wild_draw_four_mode.hpp"

#include "support/app_harness.hpp"
#include "support/app_table.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <variant>

// The strict Wild Draw Four through the application (ADR 0041): the host's setting reaches the match, an illegal Wild
// Draw Four is refused with its reason and changes nothing, and the view does not offer it.

namespace {

namespace core = uno::core;
using namespace uno::app;
using uno::testing::refusal;
using uno::testing::refusalReason;
using uno::testing::Table;

[[nodiscard]] RoomSettingsPatch strict()
{
    RoomSettingsPatch patch;
    patch.wildDrawFourMode = core::WildDrawFourMode::Strict;
    return patch;
}

// A strict room of three where the player on turn holds a Wild Draw Four AND a card of the current color: an illegal
// Wild Draw Four, which the official rules would let them bluff.
[[nodiscard]] std::unique_ptr<Table> tableWithIllegalWildDrawFour()
{
    for (std::uint64_t seed = 1; seed < 800; ++seed) {
        auto table = std::make_unique<Table>(3, seed);
        table->players.front().send(request::UpdateSettings{.settings = strict()});
        table->start();
        const auto& round = table->room().match->round();
        if (!std::holds_alternative<core::AwaitingPlay>(round.phase()) || !round.currentColor().has_value()) {
            continue;
        }
        const auto hand = round.hand(round.currentPlayer()).value_or(std::span<const core::Card>{});
        const bool holdsWildDrawFour =
            std::ranges::any_of(hand, [](const core::Card& card) { return card.rank == core::Rank::WildDrawFour; });
        const bool holdsCurrentColor =
            std::ranges::any_of(hand, [&](const core::Card& card) { return card.color == round.currentColor(); });
        if (holdsWildDrawFour && holdsCurrentColor) {
            table->clearInboxes();
            return table;
        }
    }
    throw std::logic_error{"no seed gave the player on turn an illegal Wild Draw Four"};
}

} // namespace

TEST_CASE("The host chooses the strict Wild Draw Four, and the match is played that way", "[app][strict][settings]")
{
    Table table(3, 7);
    REQUIRE(table.room().settings.wildDrawFourMode == core::WildDrawFourMode::OfficialChallenge);

    table.players.at(1).send(request::UpdateSettings{.settings = strict()});
    REQUIRE(refusal(table.players.at(1).received()) == ErrorCode::NotHost);

    table.players.front().send(request::UpdateSettings{.settings = strict()});
    table.start();

    REQUIRE(table.room().settings.wildDrawFourMode == core::WildDrawFourMode::Strict);
    REQUIRE(table.room().match->round().wildDrawFourMode() == core::WildDrawFourMode::Strict);
}

TEST_CASE("Strict: an illegal Wild Draw Four is refused with its reason and changes nothing", "[app][strict][illegal]")
{
    const auto table = tableWithIllegalWildDrawFour();
    const auto& round = table->room().match->round();
    auto& player = table->playerWithId(round.currentPlayer());
    const auto hand = round.hand(round.currentPlayer()).value_or(std::span<const core::Card>{});
    const auto wildDrawFour =
        *std::ranges::find_if(hand, [](const core::Card& card) { return card.rank == core::Rank::WildDrawFour; });
    const core::Round before = round;

    player.send(request::PlayCard{
        .cardId = wildDrawFour.id,
        .chosenColor = core::Color::Red,
        .swapTargetId = std::nullopt,
        .targetId = std::nullopt,
    });

    const auto answer = player.received();
    REQUIRE(refusal(answer) == ErrorCode::IllegalMove);
    REQUIRE(refusalReason(answer) == IllegalMoveReason::WildDrawFourIllegal);
    REQUIRE(table->room().match->round() == before);
}

TEST_CASE("Strict: the view never offers an illegal Wild Draw Four", "[app][strict][view]")
{
    const auto table = tableWithIllegalWildDrawFour();
    const auto& round = table->room().match->round();
    const auto actor = round.currentPlayer();
    const auto hand = round.hand(actor).value_or(std::span<const core::Card>{});
    const auto wildDrawFour =
        *std::ranges::find_if(hand, [](const core::Card& card) { return card.rank == core::Rank::WildDrawFour; });

    const auto view = core::project(*table->room().match, actor);

    REQUIRE(view.has_value());
    const auto& playable = view->me.playableCardIds;
    REQUIRE(std::ranges::find(playable, wildDrawFour.id) == playable.end());
    REQUIRE_FALSE(playable.empty());
}
