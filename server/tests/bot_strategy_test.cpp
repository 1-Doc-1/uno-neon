#include "uno/app/bot_level.hpp"
#include "uno/app/bot_strategy.hpp"
#include "uno/core/card.hpp"
#include "uno/core/deck.hpp"
#include "uno/core/draw_amount.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"
#include "uno/core/penalty_stacking.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/legal_actions.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// The bots (ADR 0030): they see their PlayerView and nothing else, and everything they propose is a move the engine
// accepts.

using namespace uno::core;
using uno::app::BotLevel;
using uno::app::BotView;
using uno::app::makeBotStrategy;
using uno::testing::coloredCard;
using uno::testing::deckFromHands;
using uno::testing::legalActionsOfCurrentPlayer;
using uno::testing::player;
using uno::testing::players;
using uno::testing::SeededRandomSource;
using uno::testing::startedRound;
using uno::testing::wildCard;

// A strategy is given a BotView, from which the cards of the others cannot be reached.
static_assert(!std::is_constructible_v<BotView, const Round&>);
static_assert(!std::is_constructible_v<BotView, const Match&>);
static_assert(!std::is_constructible_v<BotView, Round&>);

namespace {

constexpr std::size_t kViewsPerLevel = 1000;

[[nodiscard]] MatchSettings settingsFor(std::uint64_t seed)
{
    constexpr std::array kMultipliers{
        CardMultiplier::One,
        CardMultiplier::Two,
        CardMultiplier::Three,
        CardMultiplier::Five,
    };
    return {
        .matchLength = seed % 3 == 0 ? MatchLength::SingleRound : MatchLength::To250,
        .drawRule = seed % 2 == 0 ? DrawRule::Guided : DrawRule::Official,
        .drawAmount = (seed / 2) % 2 == 0 ? DrawAmount::UntilPlayable : DrawAmount::One,
        .declareUnoToWin = (seed / 4) % 2 == 1,
        .deck =
            {
                .drawTwo = kMultipliers.at(seed % 4),
                .wildDrawFour = kMultipliers.at((seed / 4) % 4),
                .wildDrawFive = kMultipliers.at((seed / 8) % 4),
            },
        .stacking = (seed / 3) % 2 == 1 ? PenaltyStacking::Ladder : PenaltyStacking::Official,
    };
}

[[nodiscard]] std::vector<PlayerId> catchableBy(const Round& round, const PlayerId& viewer)
{
    std::vector<PlayerId> targets;
    std::ranges::copy_if(round.unoWindows(), std::back_inserter(targets),
                         [&viewer](const PlayerId& target) { return target != viewer; });
    return targets;
}

// Seven red Fives with ids from `firstId`: a hand nobody cares about, distinct from the others.
[[nodiscard]] std::vector<Card> plainHand(std::uint32_t firstId)
{
    std::vector<Card> hand;
    hand.reserve(kHandSize);
    for (std::uint32_t slot = 0; slot < kHandSize; ++slot) {
        hand.push_back(coloredCard(firstId + slot, Color::Red, Rank::Five));
    }
    return hand;
}

[[nodiscard]] MatchProgress freshProgress(std::size_t playerCount)
{
    return {.roundNumber = 1, .scores = std::vector<std::uint32_t>(playerCount, 0), .winner = std::nullopt};
}

} // namespace

TEST_CASE("Whatever the table, a bot only proposes moves the engine accepts, and always has one on its turn", "[bot]")
{
    const auto level = GENERATE(BotLevel::Easy, BotLevel::Normal);
    const auto strategy = makeBotStrategy(level);
    SeededRandomSource botRandom{99};
    std::size_t checked = 0;

    for (std::uint64_t seed = 1; checked < kViewsPerLevel; ++seed) {
        SeededRandomSource random{seed};
        const std::size_t playerCount = 2 + (seed % 5);
        auto started = Match::start(players(playerCount), settingsFor(seed), random);
        REQUIRE(started.has_value());
        auto match = std::move(started->match);

        for (int step = 0; step < 400 && !match.winner().has_value() && checked < kViewsPerLevel; ++step) {
            if (std::holds_alternative<RoundOver>(match.round().phase())) {
                REQUIRE(match.startNextRound(random).has_value());
                continue;
            }
            const PlayerId current = match.round().currentPlayer();
            // The player on turn, and one other seat (announcing and catching are open to everybody)
            const std::array viewers{current, player((seed + static_cast<std::uint64_t>(step)) % playerCount)};
            for (const PlayerId& viewer : viewers) {
                const auto view = project(match, viewer);
                REQUIRE(view.has_value());
                const auto catchable = catchableBy(match.round(), viewer);
                const auto actions = strategy->decide(BotView{.game = *view, .catchable = catchable}, botRandom);

                Match copy = match;
                SeededRandomSource scratch{1};
                for (const PlayerAction& action : actions) {
                    CAPTURE(seed, step, viewer.value);
                    REQUIRE(copy.apply(viewer, action, scratch).has_value());
                }
                if (viewer == current) {
                    const bool movesOn = std::ranges::any_of(actions, [](const PlayerAction& action) {
                        return !std::holds_alternative<CallUno>(action) && !std::holds_alternative<CatchUno>(action);
                    });
                    CAPTURE(seed, step);
                    REQUIRE(movesOn);
                }
                ++checked;
            }

            // The table moves on with a legal move of whoever is on turn, chosen at random
            const auto legal = legalActionsOfCurrentPlayer(match.round());
            REQUIRE_FALSE(legal.empty());
            REQUIRE(match.apply(current, legal.at(random.uniform(static_cast<std::uint32_t>(legal.size()))), random)
                        .has_value());
        }
    }
}

TEST_CASE("A bot decides from its view alone: what it cannot see never changes what it does", "[bot]")
{
    const auto level = GENERATE(BotLevel::Easy, BotLevel::Normal);
    const auto strategy = makeBotStrategy(level);

    const std::vector<Card> mine{
        coloredCard(1, Color::Red, Rank::Five),
        coloredCard(2, Color::Blue, Rank::Two),
        wildCard(3, Rank::Wild),
        coloredCard(4, Color::Red, Rank::DrawTwo),
        coloredCard(5, Color::Green, Rank::Nine),
        wildCard(6, Rank::WildDrawFive),
        coloredCard(7, Color::Red, Rank::Skip),
    };
    // The same table twice: only the cards of the others and the draw pile differ
    const auto handsFor = [&mine](std::uint32_t colorShift) {
        std::vector<std::vector<Card>> hands{mine};
        for (std::uint32_t seat = 1; seat < 3; ++seat) {
            std::vector<Card> hand;
            hand.reserve(kHandSize);
            for (std::uint32_t slot = 0; slot < kHandSize; ++slot) {
                hand.push_back(coloredCard((100 * seat) + slot, kColors.at((slot + colorShift) % 4),
                                           static_cast<Rank>((slot + colorShift) % 9)));
            }
            hands.push_back(std::move(hand));
        }
        return hands;
    };
    std::array<std::optional<PlayerView>, 2> views;
    std::array<std::vector<PlayerAction>, 2> decisions;
    for (std::uint32_t variant = 0; variant < 2; ++variant) {
        SeededRandomSource random{5};
        const std::vector<Card> afterTop{
            coloredCard(900 + variant, Color::Yellow, Rank::One),
            wildCard(910 + variant, Rank::WildDrawFour),
        };
        auto round = startedRound(
            {
                .seats = players(3),
                .dealer = player(2),
                .deck = deckFromHands(handsFor(variant * 3), coloredCard(40, Color::Red, Rank::Two), afterTop),
            },
            random);
        views.at(variant) = project(round, player(0), freshProgress(3)).value();
        SeededRandomSource botRandom{11};
        decisions.at(variant) = strategy->decide(BotView{.game = *views.at(variant), .catchable = {}}, botRandom);
    }

    REQUIRE(views.at(0) == views.at(1));
    REQUIRE_FALSE(decisions.at(0).empty());
    REQUIRE(decisions.at(0) == decisions.at(1));
}

TEST_CASE("The normal bot keeps its Wilds while a plain card can be played", "[bot]")
{
    SeededRandomSource random{3};
    const std::vector<Card> mine{
        wildCard(1, Rank::Wild),
        wildCard(2, Rank::WildDrawFour),
        coloredCard(3, Color::Red, Rank::Seven),
        coloredCard(4, Color::Red, Rank::Three),
        coloredCard(5, Color::Blue, Rank::Four),
        coloredCard(6, Color::Blue, Rank::Four),
        coloredCard(7, Color::Green, Rank::Four),
    };
    auto round = startedRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck = deckFromHands({mine, plainHand(100), plainHand(200)}, coloredCard(40, Color::Red, Rank::Two)),
        },
        random);
    const auto view = project(round, player(0), freshProgress(3));
    REQUIRE(view.has_value());

    const auto actions = makeBotStrategy(BotLevel::Normal)->decide(BotView{.game = *view, .catchable = {}}, random);

    REQUIRE(actions.size() == 1);
    const auto* play = std::get_if<PlayCard>(&actions.front());
    REQUIRE(play != nullptr);
    REQUIRE(play->cardId == CardId{3}); // the red Seven: the biggest plain card that fits
}

TEST_CASE("The normal bot aims a Wild Draw Five at the player with the fewest cards", "[bot]")
{
    SeededRandomSource random{3};
    std::vector<Card> mine{wildCard(1, Rank::WildDrawFive)};
    for (std::uint32_t id = 2; id <= 7; ++id) {
        mine.push_back(coloredCard(id, Color::Yellow, Rank::One));
    }
    auto round = startedRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck = deckFromHands({mine, plainHand(100), plainHand(200)}, coloredCard(40, Color::Red, Rank::Two)),
        },
        random);
    auto view = project(round, player(0), freshProgress(3)).value();
    view.players.at(2).cardCount = 2; // seat 2 is the closest to winning

    const auto actions = makeBotStrategy(BotLevel::Normal)->decide(BotView{.game = view, .catchable = {}}, random);

    REQUIRE_FALSE(actions.empty());
    const auto* play = std::get_if<PlayCard>(&actions.back());
    REQUIRE(play != nullptr);
    REQUIRE(play->cardId == CardId{1});
    REQUIRE(play->target == player(2));
    REQUIRE(play->chosenColor == Color::Yellow); // the color it holds most
}

TEST_CASE("The normal bot catches a player who forgot to announce UNO", "[bot]")
{
    SeededRandomSource random{3};
    auto round = startedRound(
        {
            .seats = players(3),
            .dealer = player(2),
            .deck =
                deckFromHands({plainHand(10), plainHand(100), plainHand(200)}, coloredCard(40, Color::Red, Rank::Two)),
        },
        random);
    const auto view = project(round, player(0), freshProgress(3));
    REQUIRE(view.has_value());
    const std::vector<PlayerId> catchable{player(1)};

    const auto actions =
        makeBotStrategy(BotLevel::Normal)->decide(BotView{.game = *view, .catchable = catchable}, random);

    REQUIRE_FALSE(actions.empty());
    REQUIRE(actions.front() == PlayerAction{CatchUno{.target = player(1)}});
}
