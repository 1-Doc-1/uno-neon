#include "uno/core/card.hpp"
#include "uno/core/client_event.hpp"
#include "uno/core/deck.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/player_view.hpp"
#include "uno/core/scoring.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/environment.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/legal_actions.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"
#include "uno/testing/view_leak_check.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Massive random simulation (SPEC §19, phase 1): whole matches played with random legal actions, mixed
// with out-of-turn calls and arbitrary, mostly illegal, actions. After every action the engine must
// either reject it and leave the state untouched, or accept it and keep every invariant. The same
// seed always replays the same match.

using uno::core::CallUno;
using uno::core::CardId;
using uno::core::CatchUno;
using uno::core::ChooseColor;
using uno::core::Color;
using uno::core::DrawCard;
using uno::core::DrawRule;
using uno::core::Match;
using uno::core::MatchLength;
using uno::core::MatchSettings;
using uno::core::Pass;
using uno::core::PenaltyResponse;
using uno::core::PlayCard;
using uno::core::PlayerAction;
using uno::core::PlayerId;
using uno::core::RespondPenalty;
using uno::core::RoundEnded;
using uno::core::RoundOver;
using uno::testing::allCardIds;
using uno::testing::legalActionsOfCurrentPlayer;
using uno::testing::players;
using uno::testing::requireEventsLeakNothing;
using uno::testing::requireRoundInvariants;
using uno::testing::requireViewLeaksNothing;
using uno::testing::SeededRandomSource;

namespace {

constexpr std::size_t kActionLimit = 200'000; // a match that lasts longer is a livelock
constexpr std::uint32_t kOutOfTurnPercent = 15;
constexpr std::uint32_t kRemovalPerMille = 4; // a player leaves for good, now and then
constexpr std::uint32_t kPercent = 100;
constexpr std::uint32_t kDefaultGames = 20;     // keeps a local run under 2 s; the CI runs 100, the weekly job 10 000
constexpr std::uint32_t kSomeCardIdBound = 120; // a little above the 108 ids in play, to miss on purpose
constexpr std::uint32_t kColorCount = 4;

// Number of matches to simulate: UNO_SIMULATION_GAMES, 10 000 in the CI (SPEC §19), a few hundred by
// default so that the everyday test run stays quick.
[[nodiscard]] std::uint32_t gamesToSimulate()
{
    const auto value = uno::testing::environmentVariable("UNO_SIMULATION_GAMES");
    return value.has_value() ? static_cast<std::uint32_t>(std::stoul(*value)) : kDefaultGames;
}

[[nodiscard]] std::size_t playerCountFor(std::uint64_t seed)
{
    return 2 + static_cast<std::size_t>(seed % 9); // 2 to 10 players
}

[[nodiscard]] MatchLength matchLengthFor(std::uint64_t seed)
{
    constexpr std::array kLengths{MatchLength::SingleRound, MatchLength::To250, MatchLength::To500};
    return kLengths.at(static_cast<std::size_t>((seed / 9) % kLengths.size()));
}

// Both draw rules are simulated: the guided one is the default of the rooms, the official one stays available.
[[nodiscard]] DrawRule drawRuleFor(std::uint64_t seed)
{
    return seed % 2 == 0 ? DrawRule::Guided : DrawRule::Official;
}

// The "declare UNO to win" house rule is on for half of the matches, independently of the draw rule.
[[nodiscard]] bool declareUnoToWinFor(std::uint64_t seed)
{
    return (seed / 2) % 2 == 1;
}

[[nodiscard]] Match startMatch(std::uint64_t seed, SeededRandomSource& random)
{
    auto started = Match::start(players(playerCountFor(seed)),
                                MatchSettings{
                                    .matchLength = matchLengthFor(seed),
                                    .drawRule = drawRuleFor(seed),
                                    .declareUnoToWin = declareUnoToWinFor(seed),
                                },
                                random);
    REQUIRE(started.has_value());
    return std::move(started->match);
}

class Simulation {
public:
    explicit Simulation(std::uint64_t seed) : random_{seed}, match_{startMatch(seed, random_)} {}

    void run()
    {
        std::size_t actions = 0;
        while (!match_.winner().has_value()) {
            REQUIRE(++actions < kActionLimit);
            playOneAction();
            removeSomeoneSometimes();
            startNextRoundIfOver();
        }
        requireScoresAddUp();
    }

    [[nodiscard]] const Match& match() const noexcept { return match_; }

private:
    [[nodiscard]] std::uint32_t pick(std::size_t bound) { return random_.uniform(static_cast<std::uint32_t>(bound)); }

    [[nodiscard]] PlayerId anyPlayer()
    {
        const auto seats = match_.round().seats();
        return seats[pick(seats.size())]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }

    // Someone leaves the match for good: the engine must stay consistent, and with two players left it ends by forfeit.
    void removeSomeoneSometimes()
    {
        if (match_.winner().has_value() || pick(1000) >= kRemovalPerMille) {
            return;
        }
        const auto leaver = anyPlayer();
        const bool forfeit = match_.round().seats().size() == 2;
        const auto leaverScore = match_.score(leaver);
        REQUIRE(leaverScore.has_value());
        const auto result = match_.removePlayer(leaver, random_);
        REQUIRE(result.has_value());
        if (!forfeit) {
            removedPoints_ += *leaverScore;
        }
        checkState();
        checkEvents(*result);
    }

    void playOneAction()
    {
        if (pick(kPercent) < kOutOfTurnPercent) {
            applyAndCheck(anyPlayer(), anyAction());
            return;
        }
        const auto actions = legalActionsOfCurrentPlayer(match_.round());
        applyAndCheck(match_.round().currentPlayer(), actions.at(pick(actions.size())));
    }

    // Any action at all, legal or not, from the whole closed set of actions.
    [[nodiscard]] PlayerAction anyAction()
    {
        switch (pick(7)) {
        case 0:
            return PlayCard{.cardId = CardId{pick(kSomeCardIdBound)}, .chosenColor = randomOptionalColor()};
        case 1:
            return DrawCard{};
        case 2:
            return Pass{};
        case 3:
            return ChooseColor{.color = randomColor()};
        case 4:
            return RespondPenalty{.response = pick(2) == 0 ? PenaltyResponse::Accept : PenaltyResponse::Challenge};
        case 5:
            return CallUno{};
        default:
            return CatchUno{.target = anyPlayer()};
        }
    }

    [[nodiscard]] std::optional<Color> randomOptionalColor()
    {
        return pick(2) == 0 ? std::nullopt : std::optional<Color>{randomColor()};
    }

    [[nodiscard]] Color randomColor() { return uno::core::kColors.at(pick(kColorCount)); }

    // Rejected: nothing changed. Accepted: every invariant holds.
    void applyAndCheck(const PlayerId& actor, const PlayerAction& action)
    {
        const auto before = match_;
        const auto result = match_.apply(actor, action, random_);
        if (!result) {
            REQUIRE(match_ == before);
            return;
        }
        for (const auto& event : *result) {
            if (const auto* ended = std::get_if<RoundEnded>(&event)) {
                pointsScored_ += ended->points;
                requirePointsAreTheHandsLeft(ended->points);
            }
        }
        checkState();
        checkEvents(*result);
    }

    // At the very moment a round ends, its points are exactly the value of the cards left in the hands.
    void requirePointsAreTheHandsLeft(std::uint32_t points) const
    {
        std::uint32_t handsValue = 0;
        for (const auto& seated : match_.round().seats()) {
            handsValue +=
                uno::core::handPoints(match_.round().hand(seated).value_or(std::span<const uno::core::Card>{}));
        }
        REQUIRE(points == handsValue);
    }

    // Every event of an accepted action, projected for a random player, shows them nothing they may not see.
    void checkEvents(const std::vector<uno::core::DomainEvent>& events)
    {
        const auto viewer = anyPlayer();
        const auto projected = uno::core::project(events, viewer, match_.round(), match_.roundNumber());
        requireEventsLeakNothing(projected, match_.round(), viewer);
    }

    void checkState()
    {
        const auto& round = match_.round();
        requireRoundInvariants(round);
        REQUIRE(allCardIds(round).size() == uno::core::kStandardDeckSize);
        const auto viewer = anyPlayer();
        const auto view = uno::core::project(match_, viewer);
        REQUIRE(view.has_value());
        requireViewLeaksNothing(round, *view, viewer);
        requireForcedActionIsAccepted(round);
    }

    // A move the engine calls forced must be one it accepts, and only the guided draw has any. Tried on a copy, with
    // its own random source, so that the simulation itself is not disturbed.
    static void requireForcedActionIsAccepted(const uno::core::Round& round)
    {
        const auto forced = round.forcedAction();
        REQUIRE((!forced.has_value() || round.drawRule() == DrawRule::Guided));
        // A card held back only by the missing announcement is never a reason to move on for the player (ADR 0019).
        REQUIRE((!round.mustDeclareUno(round.currentPlayer()) || !forced.has_value()));
        if (forced.has_value()) {
            auto copy = round;
            SeededRandomSource scratch{1};
            REQUIRE(copy.apply(copy.currentPlayer(), *forced, scratch).has_value());
        }
    }

    void startNextRoundIfOver()
    {
        if (match_.winner().has_value() || !std::holds_alternative<RoundOver>(match_.round().phase())) {
            return;
        }
        const auto started = match_.startNextRound(random_);
        REQUIRE(started.has_value());
        checkState();
        checkEvents(*started);
    }

    // Only a round's winner scores: the points scored over the match are the scores of all players.
    void requireScoresAddUp() const
    {
        std::uint64_t total = 0;
        for (const auto& score : match_.progress().scores) {
            total += score;
        }
        REQUIRE(total + removedPoints_ == pointsScored_);
    }

    SeededRandomSource random_;
    Match match_;
    std::uint64_t pointsScored_{0};
    std::uint64_t removedPoints_{0}; // scores that left the match with the players who left
};

} // namespace

TEST_CASE("Random matches keep every engine invariant", "[core][simulation]")
{
    const auto games = gamesToSimulate();
    for (std::uint64_t seed = 0; seed < games; ++seed) {
        INFO("seed " << seed);
        Simulation simulation{seed};
        simulation.run();
        REQUIRE(simulation.match().winner().has_value());
    }
}

TEST_CASE("The same seed replays the same match", "[core][simulation]")
{
    for (const std::uint64_t seed : {7U, 8U, 9U}) {
        Simulation first{seed};
        Simulation second{seed};
        first.run();
        second.run();

        REQUIRE(first.match() == second.match());
    }
}
