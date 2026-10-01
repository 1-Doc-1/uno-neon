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
#include "uno/core/scoring.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/core/turn_phase.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
      drawPile_{std::move(drawPile)}, discardPile_{std::move(discardPile)}, currentColor_{discardPile_.top().color},
      unoCalled_(hands_.size(), false)
{
}

bool Round::hasCalledUno(const PlayerId& player) const
{
    const auto seat = turnOrder_.seatOf(player);
    return seat.has_value() && unoCalled_.at(*seat);
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
    if (std::holds_alternative<RoundOver>(phase_)) {
        return std::unexpected{DomainError::InvalidPhase};
    }
    return std::visit(
        detail::Overloaded{
            [&](const PlayCard& playCard) { return applyPlayCard(actor, playCard, random); },
            [&](const DrawCard&) { return applyDrawCard(actor, random); },
            [&](const Pass&) { return applyPass(actor); },
            [&](const ChooseColor& chooseColor) { return applyChooseColor(actor, chooseColor); },
            [&](const RespondPenalty& respondPenalty) { return applyRespondPenalty(actor, respondPenalty, random); },
            [&](const CallUno&) { return applyCallUno(actor); },
            [&](const CatchUno& catchUno) { return applyCatchUno(actor, catchUno, random); },
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

    const auto actorSeat = turnOrder_.currentSeat();
    auto& hand = hands_.at(actorSeat);
    const auto found = std::ranges::find(hand, action.cardId, &Card::id);
    if (found == hand.end()) {
        return std::unexpected{DomainError::CardNotInHand};
    }
    const Card card = *found;

    if (isWild(card.rank)) {
        if (!action.chosenColor.has_value()) {
            return std::unexpected{DomainError::ColorRequired};
        }
    } else if (action.chosenColor.has_value()) {
        return std::unexpected{DomainError::ColorNotAllowed};
    }

    if (!isPlayable(card, discardPile_.top(), currentColor_)) {
        return std::unexpected{DomainError::ColorMismatch};
    }

    // The next turn has begun: nobody can be caught for the previous play any more (ADR 0011).
    unoWindow_.reset();

    const auto previousColor = currentColor_;

    hand.erase(found);
    discardPile_.place(card);

    std::vector<DomainEvent> events{CardPlayed{.player = actor, .cardId = card.id}};
    if (isWild(card.rank)) {
        currentColor_ = action.chosenColor;
        // action.chosenColor was already checked above; re-checking here (rather than a bare *)
        // keeps this access in the same scope as its check for bugprone-unchecked-optional-access.
        if (action.chosenColor.has_value()) {
            events.emplace_back(ColorChosen{.player = actor, .color = *action.chosenColor});
        }
    } else {
        currentColor_ = card.color;
    }

    if (hand.empty()) {
        endRound(actor, card.rank, random, events);
        return events;
    }

    if (card.rank == Rank::WildDrawFour) {
        // previousColor is only ever empty while awaiting the very first color choice (ADR 0007),
        // a phase that never accepts PlayCard: it is always set here. Same pattern as the
        // chosenColor re-check above, for the same clang-tidy reason.
        if (!previousColor.has_value()) {
            throw std::logic_error{"applyPlayCard: current color unset while playing Wild Draw Four"};
        }
        const bool wasLegal = isWildDrawFourLegal(hand, *previousColor);
        // SPEC §3: the targeted player is the one who must act next — accept or challenge — so the
        // turn moves to them right away, unlike Draw Two (1.3a), which has no response window.
        turnOrder_.advance();
        phase_ = AwaitingPenaltyResponse{.wildDrawFourPlayer = actor, .wasLegal = wasLegal};
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    } else {
        phase_ = AwaitingPlay{};
        std::ranges::copy(resolveEffect(card.rank, random), std::back_inserter(events));
    }
    openUnoWindowIfNeeded(actor, actorSeat);
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
    unoWindow_.reset(); // The next turn has begun (ADR 0011).

    auto drawn = drawCards(drawPile_, discardPile_, 1, random);
    giveCards(turnOrder_.currentSeat(), drawn.cards);

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

    unoCalled_.at(turnOrder_.currentSeat()) = false; // An announcement only holds for one play.
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

std::expected<std::vector<DomainEvent>, DomainError>
Round::applyRespondPenalty(const PlayerId& actor, const RespondPenalty& action, RandomSource& random)
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    const auto* awaiting = std::get_if<AwaitingPenaltyResponse>(&phase_);
    if (awaiting == nullptr) {
        return std::unexpected{DomainError::InvalidPhase};
    }

    const PlayerId& target = actor;
    const PlayerId poser = awaiting->wildDrawFourPlayer;
    const bool wasLegal = awaiting->wasLegal;

    if (action.response == PenaltyResponse::Accept) {
        std::vector<DomainEvent> events;
        drawPenalty(target, kWildDrawFourPenaltyCards, random, events);
        turnOrder_.advance();
        events.emplace_back(PlayerSkipped{.skippedPlayer = target});
        phase_ = AwaitingPlay{};
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
        return events;
    }

    // Challenge: the poser's hand, as it was when they played the card, is revealed to the
    // challenger either way (SPEC §3) — nothing has changed it since (nobody else could act).
    const auto poserSeat = seatOfOrThrow(turnOrder_, poser);
    const std::vector<Card> revealedHand{hands_.at(poserSeat).begin(), hands_.at(poserSeat).end()};

    if (!wasLegal) {
        // Bluff confirmed: the poser pays the penalty, the challenger keeps the turn they already
        // have and plays it normally (SPEC §3) — no advance(), no PlayerSkipped.
        const ChallengeResolved verdict{
            .challenger = target,
            .challenged = poser,
            .wasBluff = true,
            .penalizedPlayer = poser,
            .penaltyAmount = kWildDrawFourPenaltyCards,
            .revealedHand = revealedHand,
        };
        std::vector<DomainEvent> events{verdict};
        drawPenalty(poser, kWildDrawFourPenaltyCards, random, events);
        phase_ = AwaitingPlay{};
        return events;
    }

    // Challenge failed: the +4 was legal, the challenger pays a heavier penalty and loses their
    // turn, exactly like accepting it would have — only the amount and the verdict event differ.
    const ChallengeResolved verdict{
        .challenger = target,
        .challenged = poser,
        .wasBluff = false,
        .penalizedPlayer = target,
        .penaltyAmount = kFailedChallengePenaltyCards,
        .revealedHand = revealedHand,
    };
    std::vector<DomainEvent> events{verdict};
    drawPenalty(target, kFailedChallengePenaltyCards, random, events);
    turnOrder_.advance();
    events.emplace_back(PlayerSkipped{.skippedPlayer = target});
    phase_ = AwaitingPlay{};
    events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    return events;
}

bool Round::canCallUno(const PlayerId& player) const
{
    const auto seat = turnOrder_.seatOf(player);
    if (!seat || unoCalled_.at(*seat)) {
        return false;
    }
    const bool inWindow = unoWindow_ == player;
    const bool aboutToPlayWithTwoCards =
        player == turnOrder_.current() && hands_.at(*seat).size() == 2 &&
        (std::holds_alternative<AwaitingPlay>(phase_) || std::holds_alternative<AwaitingDrawnCardDecision>(phase_));
    return inWindow || aboutToPlayWithTwoCards;
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyCallUno(const PlayerId& actor)
{
    const auto seat = turnOrder_.seatOf(actor);
    if (!seat) {
        return std::unexpected{DomainError::UnknownPlayer};
    }
    // A call that is not due changes nothing (SPEC §3): the client never offers the button then.
    if (!canCallUno(actor)) {
        return std::vector<DomainEvent>{};
    }
    if (unoWindow_ == actor) {
        unoWindow_.reset();
    }
    unoCalled_.at(*seat) = true;
    return std::vector<DomainEvent>{UnoCalled{.player = actor}};
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyCatchUno(const PlayerId& actor, const CatchUno& action,
                                                                          RandomSource& random)
{
    if (!turnOrder_.seatOf(actor)) {
        return std::unexpected{DomainError::UnknownPlayer};
    }
    // The first catch closes the window, so a second, simultaneous one lands here (SPEC §5).
    if (unoWindow_ != action.target) {
        return std::unexpected{DomainError::UnoWindowClosed};
    }
    if (actor == action.target) {
        return std::unexpected{DomainError::CannotCatchSelf};
    }

    std::vector<DomainEvent> events{UnoCaught{.catcher = actor, .target = action.target}};
    drawPenalty(action.target, kUnoPenaltyCards, random, events);
    return events;
}

void Round::giveCards(std::size_t seat, std::span<const Card> cards)
{
    if (cards.empty()) {
        return;
    }
    std::ranges::copy(cards, std::back_inserter(hands_.at(seat)));
    unoCalled_.at(seat) = false;
    if (unoWindow_.has_value() && turnOrder_.seatOf(*unoWindow_) == seat) {
        unoWindow_.reset();
    }
}

void Round::openUnoWindowIfNeeded(const PlayerId& player, std::size_t seat)
{
    if (hands_.at(seat).size() != 1) {
        unoCalled_.at(seat) = false;
    } else if (!unoCalled_.at(seat)) {
        unoWindow_ = player;
    }
}

void Round::drawPenalty(const PlayerId& player, std::size_t count, RandomSource& random,
                        std::vector<DomainEvent>& events)
{
    auto drawn = drawCards(drawPile_, discardPile_, count, random);
    if (drawn.reshuffled) {
        events.emplace_back(DeckReshuffled{});
    }
    giveCards(seatOfOrThrow(turnOrder_, player), drawn.cards);
    events.emplace_back(PenaltyCardsDrawn{.player = player, .cards = idsOf(drawn.cards)});
}

void Round::endRound(const PlayerId& winner, Rank rank, RandomSource& random, std::vector<DomainEvent>& events)
{
    // SPEC §3: the next player still draws for a last Draw Two or Wild Draw Four, and those cards
    // count in the score. A last Wild Draw Four cannot be challenged: nothing is left to contest.
    if (rank == Rank::DrawTwo) {
        drawPenalty(turnOrder_.next(), kDrawTwoPenaltyCards, random, events);
    } else if (rank == Rank::WildDrawFour) {
        drawPenalty(turnOrder_.next(), kWildDrawFourPenaltyCards, random, events);
    }
    std::uint32_t points = 0;
    for (const auto& hand : hands_) {
        points += handPoints(hand);
    }
    phase_ = RoundOver{.winner = winner, .points = points};
    events.emplace_back(RoundEnded{.winner = winner, .points = points});
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
        drawPenalty(target, kDrawTwoPenaltyCards, random, events);
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
        drawPenalty(target, kDrawTwoPenaltyCards, random, events);
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
