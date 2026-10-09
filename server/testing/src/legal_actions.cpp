#include "uno/testing/legal_actions.hpp"

#include "uno/core/card.hpp"
#include "uno/core/playability.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <variant>
#include <vector>

namespace uno::testing {

namespace {

void addPlaysOf(const core::Round& round, const core::Card& card, std::vector<core::PlayerAction>& actions)
{
    if (card.rank == core::Rank::WildDrawFive) {
        // A colour and a target among the other players
        for (const auto color : core::kColors) {
            for (const auto& seated : round.seats()) {
                if (seated != round.currentPlayer()) {
                    actions.emplace_back(core::PlayCard{.cardId = card.id, .chosenColor = color, .target = seated});
                }
            }
        }
    } else if (core::isWild(card.rank)) {
        for (const auto color : core::kColors) {
            actions.emplace_back(core::PlayCard{.cardId = card.id, .chosenColor = color, .target = std::nullopt});
        }
    } else {
        actions.emplace_back(core::PlayCard{.cardId = card.id, .chosenColor = std::nullopt, .target = std::nullopt});
    }
}

void addPlayableCards(const core::Round& round, std::vector<core::PlayerAction>& actions)
{
    const auto hand = round.hand(round.currentPlayer());
    REQUIRE(hand.has_value());
    for (const auto& card : *hand) {
        if (core::isPlayable(card, round.discardPile().top(), round.currentColor())) {
            addPlaysOf(round, card, actions);
        }
    }
}

} // namespace

std::vector<core::PlayerAction> legalActionsOfCurrentPlayer(const core::Round& round)
{
    std::vector<core::PlayerAction> actions;
    if (std::holds_alternative<core::AwaitingPlay>(round.phase())) {
        if (round.mustDeclareUno(round.currentPlayer())) {
            // The last card is refused until UNO is announced: announcing is the move.
            actions.emplace_back(core::CallUno{});
        } else {
            addPlayableCards(round, actions);
        }
        if (round.canDraw(round.currentPlayer())) {
            actions.emplace_back(core::DrawCard{});
        }
    } else if (const auto* drawn = std::get_if<core::AwaitingDrawnCardDecision>(&round.phase())) {
        const auto hand = round.hand(round.currentPlayer());
        REQUIRE(hand.has_value());
        for (const auto& card : *hand) {
            if (card.id == drawn->drawnCard) {
                addPlaysOf(round, card, actions);
            }
        }
        if (round.canKeepDrawnCard(round.currentPlayer())) {
            actions.emplace_back(core::Pass{});
        }
    } else if (std::holds_alternative<core::AwaitingColorChoice>(round.phase())) {
        for (const auto color : core::kColors) {
            actions.emplace_back(core::ChooseColor{.color = color});
        }
    } else if (std::holds_alternative<core::AwaitingPlusFiveResponse>(round.phase())) {
        // Accept, or answer with a Wild Draw Five (a challenge is not offered: it cannot be made)
        actions.emplace_back(core::RespondPenalty{.response = core::PenaltyResponse::Accept});
        const auto hand = round.hand(round.currentPlayer());
        REQUIRE(hand.has_value());
        for (const auto& card : *hand) {
            if (card.rank == core::Rank::WildDrawFive) {
                addPlaysOf(round, card, actions);
            }
        }
    } else if (std::holds_alternative<core::AwaitingPenaltyResponse>(round.phase())) {
        actions.emplace_back(core::RespondPenalty{.response = core::PenaltyResponse::Accept});
        actions.emplace_back(core::RespondPenalty{.response = core::PenaltyResponse::Challenge});
    }
    return actions;
}

} // namespace uno::testing
