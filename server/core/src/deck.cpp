#include "uno/core/deck.hpp"

#include "uno/core/card.hpp"
#include "uno/core/random_source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace uno::core {

namespace {

// Official composition: per color one 0 and two of each other colored rank, Draw Two aside (its count is a setting).
constexpr std::array kPlainRanksTwicePerColor{
    Rank::One,   Rank::Two,   Rank::Three, Rank::Four, Rank::Five,    Rank::Six,
    Rank::Seven, Rank::Eight, Rank::Nine,  Rank::Skip, Rank::Reverse,
};
constexpr std::size_t kDrawTwoPerColor = 2;
constexpr std::size_t kWildCopies = 4;
constexpr std::size_t kWildDrawFourCopies = 4;
constexpr std::size_t kWildDrawFiveCopies = 2;

[[nodiscard]] std::size_t times(CardMultiplier multiplier, std::size_t copies) noexcept
{
    return copies * static_cast<std::size_t>(multiplier);
}

[[nodiscard]] std::vector<Card> canonicalDeck(const DeckComposition& composition)
{
    std::vector<Card> deck;
    deck.reserve(composition.total());
    const auto add = [&deck](std::optional<Color> color, Rank rank, std::size_t copies) {
        for (std::size_t copy = 0; copy < copies; ++copy) {
            deck.push_back(Card{.id = CardId{0}, .color = color, .rank = rank});
        }
    };

    for (const auto color : kColors) {
        add(color, Rank::Zero, 1);
        for (const auto rank : kPlainRanksTwicePerColor) {
            add(color, rank, 2);
        }
        add(color, Rank::DrawTwo, composition.perColor - 1 - (2 * kPlainRanksTwicePerColor.size()));
    }
    add(std::nullopt, Rank::Wild, composition.wild);
    add(std::nullopt, Rank::WildDrawFour, composition.wildDrawFour);
    add(std::nullopt, Rank::WildDrawFive, composition.wildDrawFive);
    return deck;
}

[[nodiscard]] std::vector<CardId> shuffledIds(std::size_t count, RandomSource& random)
{
    std::vector<CardId> ids(count);
    for (std::uint32_t value = 0; auto& id : ids) {
        id = CardId{value++};
    }
    shuffle(std::span{ids}, random);
    return ids;
}

} // namespace

DeckComposition compositionOf(const DeckSettings& settings) noexcept
{
    return {
        .perColor = 1 + (2 * kPlainRanksTwicePerColor.size()) + times(settings.drawTwo, kDrawTwoPerColor),
        .wild = kWildCopies,
        .wildDrawFour = times(settings.wildDrawFour, kWildDrawFourCopies),
        .wildDrawFive = times(settings.wildDrawFive, kWildDrawFiveCopies),
    };
}

std::vector<Card> createDeck(const DeckSettings& settings, RandomSource& random)
{
    auto deck = canonicalDeck(compositionOf(settings));
    const auto ids = shuffledIds(deck.size(), random);
    for (auto&& [card, id] : std::views::zip(deck, ids)) {
        card.id = id;
    }
    return deck;
}

std::vector<Card> createStandardDeck(RandomSource& random)
{
    return createDeck(DeckSettings(), random);
}

} // namespace uno::core
