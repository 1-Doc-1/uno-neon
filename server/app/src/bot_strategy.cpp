#include "uno/app/bot_strategy.hpp"

#include "uno/core/card.hpp"
#include "uno/core/penalty_stacking.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/scoring.hpp"
#include "uno/core/turn_order.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace uno::app {
namespace {

using core::Card;
using core::CardId;
using core::Color;
using core::PlayerAction;
using core::PlayerView;
using core::Rank;

// One thing the rules let the bot do right now. The details a card needs (a color, a target) are filled in afterwards.
struct Move {
    enum class Kind : std::uint8_t { Play, Draw, Pass, Accept, Challenge, ChooseColor };

    Kind kind{};
    CardId card{}; // Play only
};

[[nodiscard]] bool isMyTurn(const PlayerView& view)
{
    return view.currentPlayerId == view.me.playerId;
}

// The moves the view offers, all of them legal: the engine computed `playableCardIds` and the "can..." flags.
[[nodiscard]] std::vector<Move> legalMoves(const PlayerView& view)
{
    const bool inPlay = view.phase != core::ViewPhase::RoundOver && view.phase != core::ViewPhase::MatchOver;
    if (!inPlay || !isMyTurn(view)) {
        return {};
    }
    const core::MyState& me = view.me;
    if (me.canChooseColor) {
        return {Move{.kind = Move::Kind::ChooseColor}};
    }
    std::vector<Move> moves;
    const auto addPlays = [&] {
        for (const CardId card : me.playableCardIds) {
            moves.push_back(Move{.kind = Move::Kind::Play, .card = card});
        }
    };
    if (me.penaltyResponse.has_value()) {
        // Taking the penalty is always allowed; answering it with a card, only when the engine says a card fits
        moves.push_back(Move{.kind = Move::Kind::Accept});
        if (me.penaltyResponse->canChallenge) {
            moves.push_back(Move{.kind = Move::Kind::Challenge});
        }
        if (me.penaltyResponse->canStack) {
            addPlays();
        }
        return moves;
    }
    addPlays();
    if (me.canDraw) {
        moves.push_back(Move{.kind = Move::Kind::Draw});
    }
    if (me.canKeepDrawnCard) {
        moves.push_back(Move{.kind = Move::Kind::Pass});
    }
    return moves;
}

[[nodiscard]] const Card* cardOf(const PlayerView& view, CardId id)
{
    const auto found = std::ranges::find(view.me.hand, id, &Card::id);
    return found == view.me.hand.end() ? nullptr : &*found;
}

[[nodiscard]] Color randomColor(core::RandomSource& random)
{
    return core::kColors.at(random.uniform(static_cast<std::uint32_t>(core::kColors.size())));
}

[[nodiscard]] std::size_t colorIndex(Color color)
{
    return static_cast<std::size_t>(std::distance(core::kColors.begin(), std::ranges::find(core::kColors, color)));
}

[[nodiscard]] std::size_t indexOfMax(const std::array<std::size_t, core::kColors.size()>& counts)
{
    return static_cast<std::size_t>(std::distance(counts.begin(), std::ranges::max_element(counts)));
}

// The color of the cards the bot holds most, apart from `except` (the card it is about to play).
[[nodiscard]] Color mostHeldColor(const PlayerView& view, std::optional<CardId> except, core::RandomSource& random)
{
    std::array<std::size_t, core::kColors.size()> counts{};
    for (const Card& card : view.me.hand) {
        if (!card.color.has_value() || card.id == except) {
            continue;
        }
        ++counts.at(colorIndex(*card.color));
    }
    const std::size_t best = indexOfMax(counts);
    if (counts.at(best) == 0) {
        return randomColor(random); // nothing but Wilds in hand: any color is as good as another
    }
    return core::kColors.at(best);
}

[[nodiscard]] std::vector<const core::SeatView*> opponentsOf(const PlayerView& view)
{
    std::vector<const core::SeatView*> opponents;
    for (const core::SeatView& seat : view.players) {
        if (seat.playerId != view.me.playerId) {
            opponents.push_back(&seat);
        }
    }
    return opponents;
}

// The opponent closest to winning: the one with the fewest cards (the first of them in seat order).
[[nodiscard]] const core::PlayerId& fewestCardsOpponent(const PlayerView& view)
{
    const auto opponents = opponentsOf(view);
    return (*std::ranges::min_element(opponents, {}, [](const core::SeatView* seat) { return seat->cardCount; }))
        ->playerId;
}

[[nodiscard]] const core::PlayerId& randomOpponent(const PlayerView& view, core::RandomSource& random)
{
    const auto opponents = opponentsOf(view);
    return opponents.at(random.uniform(static_cast<std::uint32_t>(opponents.size())))->playerId;
}

// Which player moves right after the bot, in the direction of play.
[[nodiscard]] const core::SeatView& nextPlayerAfterMe(const PlayerView& view)
{
    const auto self = std::ranges::find(view.players, view.me.playerId, &core::SeatView::playerId);
    const auto index = static_cast<std::size_t>(std::distance(view.players.begin(), self));
    const std::size_t count = view.players.size();
    const std::size_t next =
        view.direction == core::Direction::Clockwise ? (index + 1) % count : (index + count - 1) % count;
    return view.players.at(next);
}

// Builds the action of a move. `chooseColor` and `chooseTarget` are where the two strategies differ.
template <typename ColorChooser, typename TargetChooser>
[[nodiscard]] PlayerAction actionOf(const Move& move, const PlayerView& view, ColorChooser chooseColor,
                                    TargetChooser chooseTarget)
{
    switch (move.kind) {
    case Move::Kind::Draw:
        return core::DrawCard{};
    case Move::Kind::Pass:
        return core::Pass{};
    case Move::Kind::Accept:
        return core::RespondPenalty{.response = core::PenaltyResponse::Accept};
    case Move::Kind::Challenge:
        return core::RespondPenalty{.response = core::PenaltyResponse::Challenge};
    case Move::Kind::ChooseColor:
        return core::ChooseColor{.color = chooseColor(std::nullopt)};
    case Move::Kind::Play:
        break;
    }
    const Card* const card = cardOf(view, move.card);
    core::PlayCard play{.cardId = move.card, .chosenColor = std::nullopt, .target = std::nullopt};
    if (card != nullptr && core::isWild(card->rank)) {
        play.chosenColor = chooseColor(move.card);
    }
    if (card != nullptr && card->rank == Rank::WildDrawFive) {
        play.target = chooseTarget();
    }
    return play;
}

// Easy: whatever the rules allow, at random. It announces UNO now and then, so it can be caught.
class EasyBot final : public BotStrategy {
public:
    [[nodiscard]] std::vector<PlayerAction> decide(const BotView& view, core::RandomSource& random) const override
    {
        const PlayerView& game = view.game;
        std::vector<PlayerAction> actions;
        // The house rule that blocks the last card leaves no choice: it must announce
        if (game.me.mustDeclareUno || (game.me.canCallUno && random.uniform(2) == 0)) {
            actions.emplace_back(core::CallUno{});
        }
        const auto moves = legalMoves(game);
        if (!moves.empty()) {
            const Move& chosen = moves.at(random.uniform(static_cast<std::uint32_t>(moves.size())));
            actions.push_back(actionOf(
                chosen, game, [&random](std::optional<CardId> /*except*/) { return randomColor(random); },
                [&game, &random] { return randomOpponent(game, random); }));
        }
        return actions;
    }
};

// Normal: a few rules of thumb, no look-ahead.
class NormalBot final : public BotStrategy {
public:
    [[nodiscard]] std::vector<PlayerAction> decide(const BotView& view, core::RandomSource& random) const override
    {
        const PlayerView& game = view.game;
        std::vector<PlayerAction> actions;
        // Catching an announcement somebody forgot: the application only offers a window once the grace is over
        const auto victim = std::ranges::find_if(
            view.catchable, [&game](const core::PlayerId& target) { return target != game.me.playerId; });
        if (victim != view.catchable.end()) {
            actions.emplace_back(core::CatchUno{.target = *victim});
        }
        if (game.me.mustDeclareUno || game.me.canCallUno) {
            actions.emplace_back(core::CallUno{});
        }
        const auto moves = legalMoves(game);
        if (const auto chosen = choose(game, moves, random)) {
            actions.push_back(actionOf(
                *chosen, game,
                [&game, &random](std::optional<CardId> except) { return mostHeldColor(game, except, random); },
                [&game] { return fewestCardsOpponent(game); }));
        }
        return actions;
    }

private:
    [[nodiscard]] static std::optional<Move> choose(const PlayerView& game, const std::vector<Move>& moves,
                                                    core::RandomSource& random)
    {
        if (moves.empty()) {
            return std::nullopt;
        }
        std::vector<Move> plays;
        std::ranges::copy_if(moves, std::back_inserter(plays),
                             [](const Move& move) { return move.kind == Move::Kind::Play; });
        const auto has = [&moves](Move::Kind kind) {
            return std::ranges::any_of(moves, [kind](const Move& move) { return move.kind == kind; });
        };
        if (game.me.penaltyResponse.has_value()) {
            return answerPenalty(game, moves, random);
        }
        if (!plays.empty()) {
            return bestPlay(game, plays);
        }
        for (const Move::Kind kind : {Move::Kind::ChooseColor, Move::Kind::Draw, Move::Kind::Pass}) {
            if (has(kind)) {
                return Move{.kind = kind};
            }
        }
        return moves.front();
    }

    // A pending penalty: pass it on with the weakest card that can (the others stay for later), or take it. A +4 that
    // may be challenged is challenged one time in three: the bot cannot see the poser's hand.
    [[nodiscard]] static Move answerPenalty(const PlayerView& game, const std::vector<Move>& moves,
                                            core::RandomSource& random)
    {
        const Move* weakest = nullptr;
        int weakestLevel = std::numeric_limits<int>::max();
        for (const Move& move : moves) {
            const Card* const card = move.kind == Move::Kind::Play ? cardOf(game, move.card) : nullptr;
            const int level = card != nullptr ? core::penaltyLevel(card->rank).value_or(0) : 0;
            if (card != nullptr && level < weakestLevel) {
                weakest = &move;
                weakestLevel = level;
            }
        }
        if (weakest != nullptr) {
            return *weakest;
        }
        const bool mayChallenge = game.me.penaltyResponse.has_value() && game.me.penaltyResponse->canChallenge;
        return Move{.kind = mayChallenge && random.uniform(3) == 0 ? Move::Kind::Challenge : Move::Kind::Accept};
    }

    // Wilds are kept for the end (or for a +5 aimed at someone about to win); otherwise the biggest card goes first,
    // an attack when the next player is almost out of cards, and the color the bot holds most is favoured.
    struct Situation {
        std::size_t handSize{};
        std::size_t fewestOpponentCards{};
        bool nextIsClose{};
    };

    [[nodiscard]] static Move bestPlay(const PlayerView& game, const std::vector<Move>& plays)
    {
        const auto opponents = opponentsOf(game);
        const auto leader =
            std::ranges::min_element(opponents, {}, [](const core::SeatView* seat) { return seat->cardCount; });
        const Situation situation{
            .handSize = game.me.hand.size(),
            .fewestOpponentCards = (*leader)->cardCount,
            .nextIsClose = nextPlayerAfterMe(game).cardCount <= 2,
        };

        std::optional<Move> best;
        int bestScore = std::numeric_limits<int>::min();
        for (const Move& move : plays) {
            const Card* const card = cardOf(game, move.card);
            if (card == nullptr) {
                continue;
            }
            const int score = scoreOf(*card, game, situation);
            if (score > bestScore) {
                best = move;
                bestScore = score;
            }
        }
        return best.value_or(plays.front());
    }

    [[nodiscard]] static int scoreOf(const Card& card, const PlayerView& game, const Situation& situation)
    {
        constexpr int kKeepWild = -100;
        constexpr int kEndgame = 100;
        constexpr int kHitTheLeader = 160;
        if (core::isWild(card.rank)) {
            int score = kKeepWild;
            if (situation.handSize <= 2) {
                score += kEndgame;
            }
            if (card.rank == Rank::WildDrawFive && situation.fewestOpponentCards <= 2) {
                score += kHitTheLeader;
            }
            return score;
        }
        int score = static_cast<int>(core::cardPoints(card.rank));
        if (situation.nextIsClose) {
            switch (card.rank) {
            case Rank::DrawTwo:
                score += 40;
                break;
            case Rank::Skip:
                score += 30;
                break;
            case Rank::Reverse:
                score += 15;
                break;
            default:
                break;
            }
        }
        const auto sameColor = std::ranges::count(game.me.hand, card.color, &Card::color);
        return score + static_cast<int>(sameColor);
    }
};

} // namespace

std::unique_ptr<BotStrategy> makeBotStrategy(BotLevel level)
{
    switch (level) {
    case BotLevel::Easy:
        return std::make_unique<EasyBot>();
    case BotLevel::Normal:
        break;
    }
    return std::make_unique<NormalBot>();
}

} // namespace uno::app
