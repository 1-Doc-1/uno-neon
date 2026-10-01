#pragma once

#include "uno/core/card.hpp"
#include "uno/core/random_source.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace uno::core {

// Face-down pile. Cards are drawn from the top.
class DrawPile {
public:
    DrawPile() = default;
    // `cardsInDrawOrder.front()` is the first card drawn.
    explicit DrawPile(std::vector<Card> cardsInDrawOrder);

    [[nodiscard]] std::size_t size() const noexcept { return cards_.size(); }
    [[nodiscard]] bool empty() const noexcept { return cards_.empty(); }
    // Bottom card first, top card last.
    [[nodiscard]] std::span<const Card> cards() const noexcept { return cards_; }

    [[nodiscard]] std::optional<Card> drawTop();
    // Adds `cards` to the pile, then shuffles the whole pile.
    void shuffleIn(std::vector<Card> cards, RandomSource& random);
    // Slides `cards` under the pile: they will be drawn after every card already there.
    void placeUnderneath(std::vector<Card> cards);

    [[nodiscard]] bool operator==(const DrawPile&) const = default;

private:
    std::vector<Card> cards_; // top of the pile = back()
};

// Face-up pile. Never empty: it is created with the first flipped card, and taking the pile back
// for a reshuffle always leaves the top card in place.
class DiscardPile {
public:
    explicit DiscardPile(Card firstCard);

    [[nodiscard]] const Card& top() const noexcept { return cards_.back(); }
    [[nodiscard]] std::size_t size() const noexcept { return cards_.size(); }
    // Bottom card first, top card last.
    [[nodiscard]] std::span<const Card> cards() const noexcept { return cards_; }

    void place(Card card);
    // Removes and returns every card but the top one, bottom card first.
    [[nodiscard]] std::vector<Card> takeAllButTop();

    [[nodiscard]] bool operator==(const DiscardPile&) const = default;

private:
    std::vector<Card> cards_; // top of the pile = back()
};

struct DrawResult {
    std::vector<Card> cards;
    bool reshuffled{false};

    [[nodiscard]] bool operator==(const DrawResult&) const = default;
};

// Draws up to `count` cards. When the draw pile runs out, the discard pile except its top card is
// shuffled to form a new draw pile. If both piles together hold too few cards, the player draws
// what remains: fewer cards, no error (SPEC §3).
[[nodiscard]] DrawResult drawCards(DrawPile& drawPile, DiscardPile& discardPile, std::size_t count,
                                   RandomSource& random);

} // namespace uno::core
