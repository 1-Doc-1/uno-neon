#include "uno/core/round.hpp"

#include "uno/core/card.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/piles.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/turn_order.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace uno::core {

namespace {

[[nodiscard]] bool hasDuplicateIds(const std::vector<Card>& deck)
{
    std::vector<CardId> ids(deck.size());
    std::ranges::transform(deck, ids.begin(), &Card::id);
    std::ranges::sort(ids);
    return std::ranges::adjacent_find(ids) != ids.end();
}

[[nodiscard]] bool isWildDrawFour(const Card& card)
{
    return card.rank == Rank::WildDrawFour;
}

// SPEC §3: a Wild Draw Four flipped first goes back into the draw pile, which is reshuffled, and a
// new card is flipped. The loop ends as soon as the pile holds another kind of card.
[[nodiscard]] std::expected<Card, DomainError> flipStartingCard(DrawPile& drawPile, RandomSource& random)
{
    for (auto flipped = drawPile.drawTop(); flipped.has_value(); flipped = drawPile.drawTop()) {
        if (!isWildDrawFour(*flipped)) {
            return *flipped;
        }
        if (std::ranges::all_of(drawPile.cards(), isWildDrawFour)) {
            break;
        }
        drawPile.shuffleIn({*flipped}, random);
    }
    return std::unexpected{DomainError::NoValidStartingCard};
}

} // namespace

std::expected<Round, DomainError> Round::start(RoundSetup setup, RandomSource& random)
{
    auto turnOrder = TurnOrder::startingLeftOf(std::move(setup.seats), setup.dealer);
    if (!turnOrder) {
        return std::unexpected{turnOrder.error()};
    }
    const auto playerCount = turnOrder->seats().size();
    const auto dealtCount = playerCount * kHandSize;
    if (setup.deck.size() <= dealtCount) {
        return std::unexpected{DomainError::DeckTooSmall};
    }
    if (hasDuplicateIds(setup.deck)) {
        return std::unexpected{DomainError::DuplicateCard};
    }

    std::vector<Hand> hands(playerCount);
    const auto firstSeat = turnOrder->currentSeat();
    for (std::size_t dealt = 0; const auto& card : setup.deck | std::views::take(dealtCount)) {
        hands.at((firstSeat + dealt) % playerCount).push_back(card);
        ++dealt;
    }

    const auto remaining = setup.deck | std::views::drop(dealtCount);
    DrawPile drawPile{std::vector<Card>(remaining.begin(), remaining.end())};
    const auto flipped = flipStartingCard(drawPile, random);
    if (!flipped) {
        return std::unexpected{flipped.error()};
    }
    return Round{*std::move(turnOrder), std::move(setup.dealer), std::move(hands), std::move(drawPile),
                 DiscardPile{*flipped}};
}

Round::Round(TurnOrder turnOrder, PlayerId dealer, std::vector<Hand> hands, DrawPile drawPile, DiscardPile discardPile)
    : turnOrder_{std::move(turnOrder)}, dealer_{std::move(dealer)}, hands_{std::move(hands)},
      drawPile_{std::move(drawPile)}, discardPile_{std::move(discardPile)}, currentColor_{discardPile_.top().color}
{
}

std::expected<std::span<const Card>, DomainError> Round::hand(const PlayerId& player) const
{
    const auto seat = turnOrder_.seatOf(player);
    if (!seat) {
        return std::unexpected{DomainError::UnknownPlayer};
    }
    return std::span<const Card>{hands_.at(*seat)};
}

} // namespace uno::core
