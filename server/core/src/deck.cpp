#include "uno/core/deck.hpp"

#include "uno/core/card.hpp"
#include "uno/core/random_source.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace uno::core {

namespace {

// Official composition: per color one 0 and two of each other colored rank (25 cards),
// plus four Wild and four WildDrawFour.
constexpr std::array kRanksTwicePerColor{
    Rank::One,   Rank::Two,   Rank::Three, Rank::Four, Rank::Five,    Rank::Six,
    Rank::Seven, Rank::Eight, Rank::Nine,  Rank::Skip, Rank::Reverse, Rank::DrawTwo,
};
constexpr int kWildCopies = 4;

[[nodiscard]] std::vector<Card> canonicalDeck()
{
    std::vector<Card> deck;
    deck.reserve(kStandardDeckSize);
    const auto add = [&deck](std::optional<Color> color, Rank rank, int copies) {
        for (int copy = 0; copy < copies; ++copy) {
            deck.push_back(Card{.id = CardId{0}, .color = color, .rank = rank});
        }
    };

    for (const auto color : kColors) {
        add(color, Rank::Zero, 1);
        for (const auto rank : kRanksTwicePerColor) {
            add(color, rank, 2);
        }
    }
    add(std::nullopt, Rank::Wild, kWildCopies);
    add(std::nullopt, Rank::WildDrawFour, kWildCopies);
    return deck;
}

[[nodiscard]] std::vector<CardId> shuffledIds(RandomSource& random)
{
    std::vector<CardId> ids(kStandardDeckSize);
    for (std::uint32_t value = 0; auto& id : ids) {
        id = CardId{value++};
    }
    shuffle(std::span{ids}, random);
    return ids;
}

} // namespace

std::vector<Card> createStandardDeck(RandomSource& random)
{
    auto deck = canonicalDeck();
    const auto ids = shuffledIds(random);
    for (auto&& [card, id] : std::views::zip(deck, ids)) {
        card.id = id;
    }
    return deck;
}

} // namespace uno::core
