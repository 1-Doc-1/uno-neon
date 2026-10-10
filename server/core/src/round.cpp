#include "uno/core/round.hpp"

#include "uno/core/card.hpp"
#include "uno/core/detail/duplicates.hpp"
#include "uno/core/detail/overloaded.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/penalty_stacking.hpp"
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

// A Wild Draw Four or Five cannot open a round: nobody has played it, so nobody could be targeted by it.
[[nodiscard]] bool cannotStartRound(const Card& card)
{
    return card.rank == Rank::WildDrawFour || card.rank == Rank::WildDrawFive;
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

// SPEC §3: a Wild Draw Four (or Five, ADR 0028) flipped first goes back into the draw pile, which is reshuffled, and a
// new card is flipped. The loop ends as soon as the pile holds another kind of card.
[[nodiscard]] std::expected<Card, DomainError> flipStartingCard(DrawPile& drawPile, RandomSource& random)
{
    for (auto flipped = drawPile.drawTop(); flipped.has_value(); flipped = drawPile.drawTop()) {
        if (!cannotStartRound(*flipped)) {
            return *flipped;
        }
        if (std::ranges::all_of(drawPile.cards(), cannotStartRound)) {
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
    round.drawRule_ = setup.drawRule;
    round.drawAmount_ = setup.drawAmount;
    round.declareUnoToWin_ = setup.declareUnoToWin;
    round.stacking_ = setup.stacking;

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

bool Round::mustDeclareUno(const PlayerId& player) const
{
    if (!declareUnoToWin_ || player != turnOrder_.current()) {
        return false;
    }
    const auto& hand = hands_.at(turnOrder_.currentSeat());
    if (hand.size() != 1 || unoCalled_.at(turnOrder_.currentSeat())) {
        return false;
    }
    if (const auto* stack = std::get_if<AwaitingStackResponse>(&phase_)) {
        return canStackOn(hand.front().rank, stack->top); // the only card that can answer
    }
    return std::holds_alternative<AwaitingPlay>(phase_) && isPlayable(hand.front(), discardPile_.top(), currentColor_);
}

bool Round::awaitsPenaltyAnswer() const noexcept
{
    return std::holds_alternative<AwaitingPenaltyResponse>(phase_) ||
           std::holds_alternative<AwaitingStackResponse>(phase_);
}

bool Round::canStackOnPending(const PlayerId& player) const
{
    const auto* stack = std::get_if<AwaitingStackResponse>(&phase_);
    const auto seat = turnOrder_.seatOf(player);
    return stack != nullptr && seat.has_value() && player == turnOrder_.current() &&
           std::ranges::any_of(hands_.at(*seat), [&](const Card& card) { return canStackOn(card.rank, stack->top); });
}

bool Round::canDraw(const PlayerId& player) const
{
    if (player != turnOrder_.current()) {
        return false;
    }
    // Ladder (ADR 0029): there is no window to answer, a click on the draw pile takes what is owed.
    if (std::holds_alternative<AwaitingStackResponse>(phase_)) {
        return stacking_ == PenaltyStacking::Ladder;
    }
    if (!std::holds_alternative<AwaitingPlay>(phase_)) {
        return false;
    }
    if (drawRule_ == DrawRule::Official) {
        return true;
    }
    // Guided: drawing is for those who cannot play, or who prefer to keep a special card (ADR 0017).
    const auto& hand = hands_.at(turnOrder_.currentSeat());
    const auto playable = [&](const Card& card) { return isPlayable(card, discardPile_.top(), currentColor_); };
    return std::ranges::none_of(hand, playable) ||
           std::ranges::any_of(hand, [&](const Card& card) { return isSpecialCard(card) && playable(card); });
}

bool Round::canKeepDrawnCard(const PlayerId& player) const
{
    const auto* drawn = std::get_if<AwaitingDrawnCardDecision>(&phase_);
    if (drawn == nullptr || player != turnOrder_.current()) {
        return false;
    }
    if (drawRule_ == DrawRule::Official) {
        return true;
    }
    const auto& hand = hands_.at(turnOrder_.currentSeat());
    const auto card = std::ranges::find(hand, drawn->drawnCard, &Card::id);
    return card != hand.end() && isSpecialCard(*card);
}

std::optional<PlayerAction> Round::forcedAction() const
{
    if (drawRule_ != DrawRule::Guided) {
        return std::nullopt;
    }
    const auto& hand = hands_.at(turnOrder_.currentSeat());
    if (std::holds_alternative<AwaitingPlay>(phase_)) {
        const bool nothingPlayable = std::ranges::none_of(
            hand, [&](const Card& card) { return isPlayable(card, discardPile_.top(), currentColor_); });
        return nothingPlayable ? std::optional<PlayerAction>{DrawCard{}} : std::nullopt;
    }
    // A target with no card to answer with has no choice left but to draw (ADR 0028, 0029).
    if (std::holds_alternative<AwaitingStackResponse>(phase_)) {
        return canStackOnPending(turnOrder_.current())
                   ? std::nullopt
                   : std::optional<PlayerAction>{RespondPenalty{.response = PenaltyResponse::Accept}};
    }
    const auto* drawn = std::get_if<AwaitingDrawnCardDecision>(&phase_);
    if (drawn != nullptr && !canKeepDrawnCard(turnOrder_.current())) {
        return PlayCard{.cardId = drawn->drawnCard, .chosenColor = std::nullopt, .target = std::nullopt};
    }
    return std::nullopt;
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

std::expected<void, DomainError> Round::checkTarget(const Card& card, const PlayCard& action,
                                                    const PlayerId& actor) const
{
    if (card.rank != Rank::WildDrawFive) {
        return action.target.has_value() ? std::unexpected{DomainError::TargetNotAllowed}
                                         : std::expected<void, DomainError>{};
    }
    if (!action.target.has_value()) {
        return std::unexpected{DomainError::TargetRequired};
    }
    if (*action.target == actor || !turnOrder_.seatOf(*action.target).has_value()) {
        return std::unexpected{DomainError::InvalidTarget};
    }
    return {};
}

std::expected<Card, DomainError> Round::validatePlay(const PlayerId& actor, const PlayCard& action) const
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    const auto* stack = std::get_if<AwaitingStackResponse>(&phase_);
    if (const auto* awaitingDrawn = std::get_if<AwaitingDrawnCardDecision>(&phase_)) {
        if (awaitingDrawn->drawnCard != action.cardId) {
            return std::unexpected{DomainError::OnlyDrawnCardPlayable};
        }
    } else if (!std::holds_alternative<AwaitingPlay>(phase_) && stack == nullptr) {
        return std::unexpected{DomainError::InvalidPhase};
    }

    const auto actorSeat = turnOrder_.currentSeat();
    const auto& hand = hands_.at(actorSeat);
    const auto found = std::ranges::find(hand, action.cardId, &Card::id);
    if (found == hand.end()) {
        return std::unexpected{DomainError::CardNotInHand};
    }
    const Card card = *found;
    // ADR 0028, 0029: a pending penalty is only answered by a penalty card of the same level or a higher one.
    if (stack != nullptr && !canStackOn(card.rank, stack->top)) {
        return std::unexpected{stack->top == Rank::WildDrawFive ? DomainError::OnlyPlusFivePlayable
                                                                : DomainError::NotStackable};
    }

    if (isWild(card.rank) != action.chosenColor.has_value()) {
        return std::unexpected{isWild(card.rank) ? DomainError::ColorRequired : DomainError::ColorNotAllowed};
    }
    if (const auto target = checkTarget(card, action, actor); !target) {
        return std::unexpected{target.error()};
    }

    // Stacking ignores colors (ADR 0029): the card was already checked against the pending penalty.
    if (stack == nullptr && !isPlayable(card, discardPile_.top(), currentColor_)) {
        return std::unexpected{DomainError::ColorMismatch};
    }
    // House rule (ADR 0019): checked after the legality of the card, so that an unplayable card says why.
    if (declareUnoToWin_ && hand.size() == 1 && !unoCalled_.at(actorSeat)) {
        return std::unexpected{DomainError::MustDeclareUno};
    }
    return card;
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyPlayCard(const PlayerId& actor, const PlayCard& action,
                                                                          RandomSource& random)
{
    const auto validated = validatePlay(actor, action);
    if (!validated) {
        return std::unexpected{validated.error()};
    }
    const Card card = *validated;
    const auto actorSeat = turnOrder_.currentSeat();
    auto& hand = hands_.at(actorSeat);

    const auto previousColor = currentColor_;
    const auto penalty = penaltyOfPlay(card, action);

    hand.erase(std::ranges::find(hand, card.id, &Card::id));
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
    if (action.target.has_value() && penalty.has_value()) {
        events.emplace_back(PlusFiveTargeted{.player = actor, .target = penalty->target, .total = penalty->total});
    }

    if (hand.empty()) {
        endRound(actor, penalty, random, events);
        return events;
    }

    if (penalty.has_value() && (card.rank == Rank::WildDrawFive || stacking_ == PenaltyStacking::Ladder)) {
        // The target must answer before anything else happens: the turn moves to them right away, wherever they sit.
        // A Draw Two or a Wild Draw Four does the same with the ladder, and then cannot be contested (ADR 0029).
        turnOrder_.moveTo(penalty->target);
        phase_ = AwaitingStackResponse{.total = penalty->total, .top = card.rank};
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    } else if (card.rank == Rank::WildDrawFour) {
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

std::optional<Penalty> Round::penaltyOfPlay(const Card& card, const PlayCard& action) const
{
    const auto* stack = std::get_if<AwaitingStackResponse>(&phase_);
    const std::size_t alreadyOwed = stack != nullptr ? stack->total : 0;
    if (card.rank == Rank::WildDrawFive && action.target.has_value()) {
        return Penalty{.target = *action.target, .total = alreadyOwed + kWildDrawFivePenaltyCards};
    }
    if (card.rank == Rank::DrawTwo || card.rank == Rank::WildDrawFour) {
        return Penalty{.target = turnOrder_.next(), .total = alreadyOwed + penaltyCards(card.rank)};
    }
    return std::nullopt;
}

std::expected<std::vector<DomainEvent>, DomainError> Round::applyDrawCard(const PlayerId& actor, RandomSource& random)
{
    if (actor != turnOrder_.current()) {
        return std::unexpected{DomainError::NotYourTurn};
    }
    if (const auto* stack = std::get_if<AwaitingStackResponse>(&phase_); stack != nullptr && canDraw(actor)) {
        return acceptStack(actor, stack->total, random);
    }
    if (!std::holds_alternative<AwaitingPlay>(phase_)) {
        return std::unexpected{DomainError::InvalidPhase};
    }
    if (!canDraw(actor)) {
        return std::unexpected{DomainError::MustPlay};
    }

    const auto seat = turnOrder_.currentSeat();
    const auto isPlayableNow = [&](const Card& card) { return isPlayable(card, discardPile_.top(), currentColor_); };
    // ADR 0024: only a player with nothing to play keeps drawing; a voluntary draw is a single card.
    const bool untilPlayable =
        drawAmount_ == DrawAmount::UntilPlayable && std::ranges::none_of(hands_.at(seat), isPlayableNow);

    std::vector<DomainEvent> events;
    std::optional<CardId> playableCard;
    bool drewAnything = false;
    // One event per card, so that the client can pace them. Stops at a playable card, or when both piles are empty.
    while (true) {
        auto drawn = drawCards(drawPile_, discardPile_, 1, random);
        if (drawn.reshuffled) {
            events.emplace_back(DeckReshuffled{});
        }
        if (drawn.cards.empty()) {
            if (!drewAnything) {
                events.emplace_back(CardsDrawn{.player = actor, .cards = {}});
            }
            break;
        }
        drewAnything = true;
        giveCards(seat, drawn.cards);
        events.emplace_back(CardsDrawn{.player = actor, .cards = idsOf(drawn.cards)});
        if (isPlayableNow(drawn.cards.front())) {
            playableCard = drawn.cards.front().id;
        }
        if (!untilPlayable || playableCard.has_value()) {
            break;
        }
    }

    // SPEC §3: the drawn card may be played immediately only if it is playable; otherwise (and
    // when nothing was left to draw) the turn ends right away instead of waiting for a Pass.
    if (playableCard.has_value()) {
        phase_ = AwaitingDrawnCardDecision{.drawnCard = *playableCard};
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
    if (!canKeepDrawnCard(actor)) {
        return std::unexpected{DomainError::MustPlay};
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
    if (const auto* plusFive = std::get_if<AwaitingStackResponse>(&phase_)) {
        return answerStack(actor, action, plusFive->total, random);
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

std::expected<std::vector<DomainEvent>, DomainError>
Round::answerStack(const PlayerId& target, const RespondPenalty& action, std::size_t total, RandomSource& random)
{
    // Nothing is left to contest: a penalty on the stack is what it says it is (ADR 0028, 0029). Answering it is a
    // PlayCard.
    if (action.response == PenaltyResponse::Challenge) {
        return std::unexpected{DomainError::CannotChallenge};
    }
    return acceptStack(target, total, random);
}

std::vector<DomainEvent> Round::acceptStack(const PlayerId& target, std::size_t total, RandomSource& random)
{
    std::vector<DomainEvent> events;
    drawPenalty(target, total, random, events);
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
    const bool downToOneCard = hands_.at(*seat).size() == 1;
    const bool aboutToPlayWithTwoCards =
        player == turnOrder_.current() && hands_.at(*seat).size() == 2 &&
        (std::holds_alternative<AwaitingPlay>(phase_) || std::holds_alternative<AwaitingDrawnCardDecision>(phase_) ||
         std::holds_alternative<AwaitingStackResponse>(phase_));
    return downToOneCard || aboutToPlayWithTwoCards;
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
    closeUnoWindow(actor);
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
    if (!hasUnoWindowOn(action.target)) {
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
    closeUnoWindow(turnOrder_.seats().subspan(seat, 1).front());
}

bool Round::hasUnoWindowOn(const PlayerId& player) const
{
    return std::ranges::find(unoWindows_, player) != unoWindows_.end();
}

void Round::closeUnoWindow(const PlayerId& player)
{
    std::erase(unoWindows_, player);
}

void Round::openUnoWindowIfNeeded(const PlayerId& player, std::size_t seat)
{
    if (hands_.at(seat).size() != 1) {
        unoCalled_.at(seat) = false;
    } else if (!unoCalled_.at(seat) && !hasUnoWindowOn(player)) {
        unoWindows_.push_back(player);
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

void Round::endRound(const PlayerId& winner, const std::optional<Penalty>& penalty, RandomSource& random,
                     std::vector<DomainEvent>& events)
{
    // SPEC §3: the target still draws for a last Draw Two, Wild Draw Four or Wild Draw Five, and those cards count in
    // the score. Nothing is left to contest or to answer with, so the whole stack falls on them (ADR 0028, 0029).
    if (penalty.has_value()) {
        drawPenalty(penalty->target, penalty->total, random, events);
    }
    std::uint32_t points = 0;
    for (const auto& hand : hands_) {
        points += handPoints(hand);
    }
    phase_ = RoundOver{.winner = winner, .points = points};
    unoWindows_.clear();
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

std::expected<std::vector<DomainEvent>, DomainError> Round::removePlayer(const PlayerId& player, RandomSource& random)
{
    const auto seat = turnOrder_.seatOf(player);
    if (!seat.has_value()) {
        return std::unexpected{DomainError::UnknownPlayer};
    }
    const bool wasCurrent = turnOrder_.current() == player;
    const auto seatsBefore = turnOrder_.seats();
    const PlayerId previousSeatPlayer = *std::next(
        seatsBefore.begin(), static_cast<std::ptrdiff_t>((*seat + seatsBefore.size() - 1) % seatsBefore.size()));
    if (!turnOrder_.remove(player)) {
        return std::unexpected{DomainError::NotEnoughPlayers};
    }

    drawPile_.placeUnderneath(std::move(hands_.at(*seat)));
    hands_.erase(hands_.begin() + static_cast<std::ptrdiff_t>(*seat));
    unoCalled_.erase(unoCalled_.begin() + static_cast<std::ptrdiff_t>(*seat));
    closeUnoWindow(player);
    if (dealer_ == player) {
        dealer_ = previousSeatPlayer;
    }

    std::vector<DomainEvent> events;
    const auto* penalty = std::get_if<AwaitingPenaltyResponse>(&phase_);
    const bool penaltyVoid = penalty != nullptr && (wasCurrent || penalty->wildDrawFourPlayer == player);
    // A stack of penalties is void when its target leaves: play resumes with whoever follows them (ADR 0028, 0029)
    const bool stackVoid = wasCurrent && std::holds_alternative<AwaitingStackResponse>(phase_);
    if (penaltyVoid || stackVoid) {
        phase_ = AwaitingPlay{};
    }
    if (wasCurrent && !std::holds_alternative<RoundOver>(phase_)) {
        if (std::holds_alternative<AwaitingColorChoice>(phase_)) {
            const Color color = kColors.at(random.uniform(static_cast<std::uint32_t>(kColors.size())));
            currentColor_ = color;
            events.emplace_back(ColorChosen{.player = player, .color = color});
        }
        if (std::holds_alternative<AwaitingDrawnCardDecision>(phase_) ||
            std::holds_alternative<AwaitingColorChoice>(phase_)) {
            phase_ = AwaitingPlay{};
        }
        events.emplace_back(TurnChanged{.player = turnOrder_.current()});
    }
    return events;
}

} // namespace uno::core
