#include "uno/core/piles.hpp"

#include "uno/core/card.hpp"
#include "uno/core/random_source.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace uno::core {

DrawPile::DrawPile(std::vector<Card> cardsInDrawOrder) : cards_{std::move(cardsInDrawOrder)}
{
    std::ranges::reverse(cards_);
}

std::optional<Card> DrawPile::drawTop()
{
    if (cards_.empty()) {
        return std::nullopt;
    }
    Card top = cards_.back();
    cards_.pop_back();
    return top;
}

void DrawPile::shuffleIn(std::vector<Card> cards, RandomSource& random)
{
    std::ranges::copy(cards, std::back_inserter(cards_));
    shuffle(std::span{cards_}, random);
}

DiscardPile::DiscardPile(Card firstCard) : cards_{firstCard} {}

void DiscardPile::place(Card card)
{
    cards_.push_back(card);
}

std::vector<Card> DiscardPile::takeAllButTop()
{
    const auto top = std::prev(cards_.end());
    std::vector<Card> taken(cards_.begin(), top);
    cards_.erase(cards_.begin(), top);
    return taken;
}

DrawResult drawCards(DrawPile& drawPile, DiscardPile& discardPile, std::size_t count, RandomSource& random)
{
    DrawResult result;
    result.cards.reserve(std::min(count, drawPile.size() + discardPile.size() - 1));
    while (result.cards.size() < count) {
        if (drawPile.empty()) {
            auto reshuffledCards = discardPile.takeAllButTop();
            if (reshuffledCards.empty()) {
                break;
            }
            drawPile.shuffleIn(std::move(reshuffledCards), random);
            result.reshuffled = true;
        }
        // drawPile is non-empty here: either it already was, or the reshuffle above just refilled
        // it. A bare `*` would be UB if that invariant were ever broken, and would silently stop
        // growing result.cards (an infinite loop, since drawPile.empty() would still be false). The
        // explicit has_value() check makes the failure loud instead, and keeps clang-tidy's
        // bugprone-unchecked-optional-access happy: it accepts operator* once the same optional has
        // been checked, but not .value() alone or an unchecked operator*.
        auto drawn = drawPile.drawTop();
        if (!drawn.has_value()) {
            throw std::logic_error{"drawCards: draw pile unexpectedly empty after a successful reshuffle"};
        }
        result.cards.push_back(*drawn);
    }
    return result;
}

} // namespace uno::core
