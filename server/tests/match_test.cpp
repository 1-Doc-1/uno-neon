#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/match.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_phase.hpp"
#include "uno/testing/fixtures.hpp"
#include "uno/testing/legal_actions.hpp"
#include "uno/testing/round_invariants.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

using uno::core::DomainError;
using uno::core::DomainEvent;
using uno::core::DrawCard;
using uno::core::Match;
using uno::core::MatchEnded;
using uno::core::MatchLength;
using uno::core::MatchSettings;
using uno::core::RoundOver;
using uno::core::RoundStarted;
using uno::testing::legalActionsOfCurrentPlayer;
using uno::testing::player;
using uno::testing::players;
using uno::testing::requireRoundInvariants;
using uno::testing::SeededRandomSource;

namespace {

constexpr std::uint64_t kSeed = 42;
constexpr std::size_t kStepLimit = 20'000;
constexpr std::uint32_t kRoundLimit = 200;
constexpr std::uint32_t kTo250Points = 250;

// Plays the first legal action of the current player (a card whenever one is playable, otherwise a draw)
// until the round is over, and returns the events of the final action. A real round ends long before
// kStepLimit.
[[nodiscard]] std::vector<DomainEvent> playRoundToTheEnd(Match& match, SeededRandomSource& random)
{
    std::vector<DomainEvent> lastEvents;
    for (std::size_t step = 0; step < kStepLimit && !std::holds_alternative<RoundOver>(match.round().phase()); ++step) {
        const auto actions = legalActionsOfCurrentPlayer(match.round());
        REQUIRE(!actions.empty());
        auto result = match.apply(match.round().currentPlayer(), actions.front(), random);
        REQUIRE(result.has_value());
        lastEvents = *std::move(result);
        requireRoundInvariants(match.round());
    }
    REQUIRE(std::holds_alternative<RoundOver>(match.round().phase()));
    return lastEvents;
}

// Plays one whole round and checks that the match is over exactly when the round winner reached
// `target`. Returns whether the match is over.
[[nodiscard]] bool playRoundAndCheckScore(Match& match, SeededRandomSource& random, std::uint32_t target)
{
    const auto lastEvents = playRoundToTheEnd(match, random);
    const auto* over = std::get_if<RoundOver>(&match.round().phase());
    REQUIRE(over != nullptr);
    const bool targetReached = match.score(over->winner).value_or(0) >= target;
    REQUIRE(match.winner().has_value() == targetReached);
    REQUIRE(std::holds_alternative<MatchEnded>(lastEvents.back()) == targetReached);
    if (targetReached) {
        REQUIRE(match.winner() == over->winner);
    }
    return targetReached;
}

[[nodiscard]] Match startMatch(std::size_t playerCount, SeededRandomSource& random,
                               MatchSettings settings = MatchSettings{})
{
    auto started = Match::start(players(playerCount), settings, random);
    REQUIRE(started.has_value());
    REQUIRE(std::holds_alternative<RoundStarted>(started->events.front()));
    return std::move(started->match);
}

} // namespace

TEST_CASE("A match needs between 2 and 10 distinct players", "[core][match]")
{
    SeededRandomSource random{kSeed};

    REQUIRE(Match::start(players(1), MatchSettings{}, random).error() == DomainError::NotEnoughPlayers);
    REQUIRE(Match::start({}, MatchSettings{}, random).error() == DomainError::NotEnoughPlayers);
    REQUIRE(Match::start(players(11), MatchSettings{}, random).error() == DomainError::TooManyPlayers);
    REQUIRE(Match::start({player(0), player(0)}, MatchSettings{}, random).error() == DomainError::DuplicatePlayer);
}

TEST_CASE("A new match starts at round 1 with every score at zero", "[core][match]")
{
    SeededRandomSource random{kSeed};

    const auto match = startMatch(4, random);

    REQUIRE(match.roundNumber() == 1);
    REQUIRE(match.winner() == std::nullopt);
    REQUIRE(match.settings().matchLength == MatchLength::To500);
    for (std::size_t seat = 0; seat < 4; ++seat) {
        REQUIRE(match.score(player(seat)) == 0);
    }
    REQUIRE(match.score(player(9)).error() == DomainError::UnknownPlayer);
}

TEST_CASE("In a single-round match, the round winner wins the match", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(3, random, {.matchLength = MatchLength::SingleRound});

    const auto lastEvents = playRoundToTheEnd(match, random);

    const auto* over = std::get_if<RoundOver>(&match.round().phase());
    REQUIRE(over != nullptr);
    REQUIRE(match.winner() == over->winner);
    REQUIRE(match.score(over->winner) == over->points);
    REQUIRE(std::holds_alternative<MatchEnded>(lastEvents.back()));
    REQUIRE(std::get<MatchEnded>(lastEvents.back()).winner == over->winner);
}

TEST_CASE("Only the round winner scores, and the match goes on below the target", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(3, random);

    const auto lastEvents = playRoundToTheEnd(match, random);

    const auto* over = std::get_if<RoundOver>(&match.round().phase());
    REQUIRE(over != nullptr);
    REQUIRE(match.winner() == std::nullopt);
    REQUIRE(!std::holds_alternative<MatchEnded>(lastEvents.back()));
    for (std::size_t seat = 0; seat < 3; ++seat) {
        const auto expected = player(seat) == over->winner ? over->points : 0U;
        REQUIRE(match.score(player(seat)) == expected);
    }
}

TEST_CASE("A match lasts until the round winner reaches the target", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(3, random, {.matchLength = MatchLength::To250});

    for (std::uint32_t round = 1; round <= kRoundLimit; ++round) {
        REQUIRE(match.roundNumber() == round);
        if (playRoundAndCheckScore(match, random, kTo250Points)) {
            return;
        }
        REQUIRE(match.startNextRound(random).has_value());
    }
    FAIL("no player reached 250 points in " << kRoundLimit << " rounds");
}

TEST_CASE("The next round rotates the dealer, keeps the scores and bumps the round number", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(4, random);
    const auto firstDealer = match.round().dealer();
    (void)playRoundToTheEnd(match, random);
    const auto* over = std::get_if<RoundOver>(&match.round().phase());
    REQUIRE(over != nullptr);
    const auto winner = over->winner;
    const auto winnerScore = match.score(winner);

    const auto events = match.startNextRound(random);

    REQUIRE(events.has_value());
    REQUIRE(std::holds_alternative<RoundStarted>(events->front()));
    REQUIRE(match.roundNumber() == 2);
    const std::vector<uno::core::PlayerId> seats{match.round().seats().begin(), match.round().seats().end()};
    const auto firstDealerSeat = static_cast<std::size_t>(std::ranges::find(seats, firstDealer) - seats.begin());
    REQUIRE(match.round().dealer() == seats.at((firstDealerSeat + 1) % 4));
    REQUIRE(match.score(winner) == winnerScore);
    REQUIRE(!std::holds_alternative<RoundOver>(match.round().phase()));
    requireRoundInvariants(match.round());
}

TEST_CASE("The next round cannot start while the current one is in progress", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(2, random);

    REQUIRE(match.startNextRound(random).error() == DomainError::InvalidPhase);
}

TEST_CASE("Nothing can be played, or dealt, once the match is over", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(2, random, {.matchLength = MatchLength::SingleRound});
    (void)playRoundToTheEnd(match, random);

    REQUIRE(match.startNextRound(random).error() == DomainError::InvalidPhase);
    REQUIRE(match.apply(player(0), DrawCard{}, random).error() == DomainError::InvalidPhase);
}

TEST_CASE("Several rounds in a row keep every invariant", "[core][match]")
{
    SeededRandomSource random{kSeed};
    auto match = startMatch(3, random, {.matchLength = MatchLength::To250});

    for (std::uint32_t expectedRound = 1; expectedRound <= 3 && !match.winner().has_value(); ++expectedRound) {
        REQUIRE(match.roundNumber() == expectedRound);
        (void)playRoundToTheEnd(match, random);
        if (!match.winner().has_value()) {
            REQUIRE(match.startNextRound(random).has_value());
        }
    }
}
