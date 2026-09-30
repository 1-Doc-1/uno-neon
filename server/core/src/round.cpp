#include "uno/core/round.hpp"

#include "uno/core/card.hpp"
#include "uno/core/detail/duplicates.hpp"
#include "uno/core/detail/overloaded.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/piles.hpp"
#include "uno/core/playability.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/core/turn_phase.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace uno::core {

namespace {

[[nodiscard]] bool hasDuplicateIds(const std::vector<Card>& deck)
{
    std::vector<CardId> ids(deck.size());
    std::ranges::transform(deck, ids.begin(), &Card::id);
    return detail::hasDuplicates(std::move(ids));
}

[[nodiscard]] bool isWildDrawFour(const Card& card)
{
    return card.rank == Rank::WildDrawFour;
}

[[nodiscard]] std::vector<CardId> idsOf(const std::vector<Card>& cards)
{
    std::vector<CardId> ids(cards.size());
    std::ranges::transform(cards, ids.begin(), &Card::id);
    return ids;
}

// `player` always comes from `turnOrder`'s own current()/next(), so it is always seated: the
// exception below can only fire on a logic error in this file, never on player input.
[[nodiscard]] std::size_t seatOfOrThrow(const TurnOrder& turnOrder, const PlayerId& player)
{
    const auto seat = turnOrder.seatOf(player);
    if (!seat.has_value()) {
        throw std::logic_error{"seatOfOrThrow: player is not seated in this round"};
    }
    return *seat;
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

std::expected<RoundStart, DomainError> Round::start(RoundSetup setup, RandomSource& random)
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
    for (std::size_t dealt = 0; dealt < dealtCount; ++dealt) {
        hands.at((firstSeat + dealt) % playerCount).push_back(setup.deck.at(dealt));
    }

    const auto remaining = setup.deck | std::views::drop(dealtCount);
    DrawPile drawPile{std::vector<Card>(remaining.begin(), remaining.end())};
    const auto flipped = flipStartingCard(drawPile, random);
    if (!flipped) {
        return std::unexpected{flipped.error()};
    }

    const PlayerId dealer = setup.dealer;
    Round round{*std::move(turnOrder), std::move(setup.dealer), std::move(hands), std::move(drawPile),
                DiscardPile{*flipped}};

    std::vector<DomainEvent> events{RoundStarted{.dealer = dealer, .firstCard = *flipped}};
    if (flipped->rank == Rank::Wild) {
        // No real actor yet: the first player must choose a color before their own turn even
        // starts (ADR 0007). Unlike Skip/Reverse/Draw Two below, nobody's turn is consumed.
        round.phase_ = AwaitingColorChoice{};
    } else {
        std::ranges::copy(round.resolveFirstCardEffect(flipped->rank, random), std::back_inserter(events));
    }
    return RoundStart{.round = std::move(round), .events = std::move(events)};
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

std::expected<std::vector<DomainEvent>, DomainError> Round::apply(const PlayerId& actor, const PlayerAction& action,
                                                                  RandomSource& random)
{
    return std::visit(detail::Overloaded{
                          [&](const PlayCard& playCard) { return applyPlayCard(actor, playCard, random); },
                          [&](const DrawCard&) { return applyDrawCard(actor, random); },
                          [&](const Pass&) { return applyPass(actor); },
                          [&](const ChooseColor& chooseColor) { return applyChooseColor(actor, chooseColor); },
                      },
                      action);
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyPlayCard(const PlayerId& actor, const PlayCard& action,
                                                                          RandomSource& random)
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    if (const auto* awaitingDrawn = std::get_if<AwaitingDrawnCardDecision>(&phase_)) {
        if (awaitingDrawn->drawnCard != action.cardId) {
            return std::unexpected{DomainError::OnlyDrawnCardPlayable};
        }
    } else if (!std::holds_alternative<AwaitingPlay>(phase_)) {
        return std::unexpected{DomainError::InvalidPhase};
    }

    auto& hand = hands_.at(turnOrder_.currentSeat());
    const auto found = std::ranges::find(hand, action.cardId, &Card::id);
    if (found == hand.end()) {
        return std::unexpected{DomainError::CardNotInHand};
    }
    const Card card = *found;

    if (card.rank == Rank::Wild) {
        if (!action.chosenColor.has_value()) {
            return std::unexpected{DomainError::ColorRequired};
        }
    } else if (action.chosenColor.has_value()) {
        return std::unexpected{DomainError::ColorNotAllowed};
    }

    if (!isPlayable(card, discardPile_.top(), currentColor_)) {
        return std::unexpected{DomainError::ColorMismatch};
    }

    hand.erase(found);
    discardPile_.place(card);
    phase_ = AwaitingPlay{};

    std::vector<DomainEvent> events{CardPlayed{.player = actor, .cardId = card.id}};
    if (card.rank == Rank::Wild) {
        currentColor_ = action.chosenColor;
        // action.chosenColor was already checked above; re-checking here (rather than a bare *)
        // keeps this access in the same scope as its check for bugprone-unchecked-optional-access.
        if (action.chosenColor.has_value()) {
            events.emplace_back(ColorChosen{.player = actor, .color = *action.chosenColor});
        }
    } else {
        currentColor_ = card.color;
    }
    std::ranges::copy(resolveEffect(card.rank, random), std::back_inserter(events));
    return events;
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyDrawCard(const PlayerId& actor, RandomSource& random)
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    if (!std::holds_alternative<AwaitingPlay>(phase_)) {
        return std::unexpected{DomainError::InvalidPhase};
    }

    auto drawn = drawCards(drawPile_, discardPile_, 1, random);
    std::ranges::copy(drawn.cards, std::back_inserter(hands_.at(turnOrder_.currentSeat())));

    std::vector<DomainEvent> events;
    if (drawn.reshuffled) {
        events.emplace_back(DeckReshuffled{});
    }
    events.emplace_back(CardsDrawn{.player = actor, .cards = idsOf(drawn.cards)});

    // SPEC §3: the drawn card may be played immediately only if it is playable; otherwise (and
    // when nothing was left to draw) the turn ends right away instead of waiting for a Pass.
    const bool playable = !drawn.cards.empty() && isPlayable(drawn.cards.front(), discardPile_.top(), currentColor_);
    if (playable) {
        phase_ = AwaitingDrawnCardDecision{.drawnCard = drawn.cards.front().id};
    } else {
        events.emplace_back(TurnPassed{.player = actor});
        turnOrder_.advance();
        phase_ = AwaitingPlay{};
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    }
    return events;
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyPass(const PlayerId& actor)
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    if (!std::holds_alternative<AwaitingDrawnCardDecision>(phase_)) {
        return std::unexpected{DomainError::InvalidPhase};
    }

    std::vector<DomainEvent> events{TurnPassed{.player = actor}};
    turnOrder_.advance();
    phase_ = AwaitingPlay{};
    events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    return events;
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyChooseColor(const PlayerId& actor,
                                                                             const ChooseColor& action)
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    if (!std::holds_alternative<AwaitingColorChoice>(phase_)) {
        return std::unexpected{DomainError::InvalidPhase};
    }

    currentColor_ = action.color;
    phase_ = AwaitingPlay{};
    // No TurnChanged: the first player keeps the turn, they simply play normally next.
    return std::vector<DomainEvent>{ColorChosen{.player = actor, .color = action.color}};
}

std::vector<DomainEvent> Round::resolveEffect(Rank rank, RandomSource& random)
{
    std::vector<DomainEvent> events;
    switch (rank) {
    case Rank::Skip: {
        const auto skipped = turnOrder_.next();
        turnOrder_.advance();
        turnOrder_.advance();
        events.emplace_back(PlayerSkipped{.skippedPlayer = skipped});
        break;
    }
    case Rank::Reverse: {
        const auto opponentIfTwoPlayers = turnOrder_.next();
        turnOrder_.reverse();
        turnOrder_.advance();
        // SPEC §3: with only two players, Reverse acts as a Skip (the other direction leads back
        // to the same next player, so a second advance hands the turn back to the actor).
        if (turnOrder_.seats().size() == 2) {
            turnOrder_.advance();
            events.emplace_back(PlayerSkipped{.skippedPlayer = opponentIfTwoPlayers});
        } else {
            events.emplace_back(DirectionReversed{});
        }
        break;
    }
    case Rank::DrawTwo: {
        const auto target = turnOrder_.next();
        const auto targetSeat = seatOfOrThrow(turnOrder_, target);
        auto drawn = drawCards(drawPile_, discardPile_, 2, random);
        if (drawn.reshuffled) {
            events.emplace_back(DeckReshuffled{});
        }
        std::ranges::copy(drawn.cards, std::back_inserter(hands_.at(targetSeat)));
        events.emplace_back(PenaltyCardsDrawn{.player = target, .cards = idsOf(drawn.cards)});
        turnOrder_.advance();
        turnOrder_.advance();
        events.emplace_back(PlayerSkipped{.skippedPlayer = target});
        break;
    }
    default:
        // Plain colored/numbered card, or Wild: the turn simply moves on.
        turnOrder_.advance();
        break;
    }
    events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    return events;
}

std::vector<DomainEvent> Round::resolveFirstCardEffect(Rank rank, RandomSource& random)
{
    std::vector<DomainEvent> events;
    switch (rank) {
    case Rank::Skip: {
        const auto skipped = turnOrder_.current();
        turnOrder_.advance();
        events.emplace_back(PlayerSkipped{.skippedPlayer = skipped});
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
        break;
    }
    case Rank::Reverse: {
        turnOrder_.reverse();
        turnOrder_.advance();
        events.emplace_back(DirectionReversed{});
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
        break;
    }
    case Rank::DrawTwo: {
        const auto target = turnOrder_.current();
        const auto targetSeat = seatOfOrThrow(turnOrder_, target);
        auto drawn = drawCards(drawPile_, discardPile_, 2, random);
        if (drawn.reshuffled) {
            events.emplace_back(DeckReshuffled{});
        }
        std::ranges::copy(drawn.cards, std::back_inserter(hands_.at(targetSeat)));
        events.emplace_back(PenaltyCardsDrawn{.player = target, .cards = idsOf(drawn.cards)});
        turnOrder_.advance();
        events.emplace_back(PlayerSkipped{.skippedPlayer = target});
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
        break;
    }
    default:
        break; // Plain colored/numbered card: no effect. Wild is handled by start() itself.
    }
    return events;
}

} // namespace uno::core
