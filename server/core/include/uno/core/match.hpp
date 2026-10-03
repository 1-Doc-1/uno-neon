#pragma once

#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/round.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace uno::core {

// How long a match lasts (SPEC §4, ADR 0007 #3): one round, or until a player reaches 250 or 500 points.
enum class MatchLength : std::uint8_t { SingleRound, To250, To500 };

// The score a player must reach to win the match, or empty when a single round decides it.
[[nodiscard]] constexpr std::optional<std::uint32_t> targetPoints(MatchLength length) noexcept
{
    switch (length) {
    case MatchLength::SingleRound:
        return std::nullopt;
    case MatchLength::To250:
        return 250U;
    case MatchLength::To500:
        return 500U;
    }
    return std::nullopt;
}

struct MatchSettings {
    MatchLength matchLength{MatchLength::To500};
    DrawRule drawRule{DrawRule::Official};
    bool declareUnoToWin{false}; // ADR 0019

    [[nodiscard]] bool operator==(const MatchSettings&) const = default;
};

// What a match has accumulated beyond its current round: the numbers a player sees next to the table.
struct MatchProgress {
    std::uint32_t roundNumber{1};
    std::vector<std::uint32_t> scores; // indexed by seat
    std::optional<PlayerId> winner;    // empty until the match is over

    [[nodiscard]] bool operator==(const MatchProgress&) const = default;
};

struct MatchStart;

// Chains rounds, rotates the dealer and keeps the scores (SPEC §7.1). Like Round, a plain value that
// owns no dependency: the RandomSource is a parameter of every operation that deals or shuffles.
// The winner of a round is the only one to score, so at most one player can cross the target.
class Match {
public:
    // Picks the first dealer at random among `seats` (listed clockwise), shuffles a standard deck and
    // starts the first round. Rejects fewer than 2 or more than 10 players, or duplicates.
    [[nodiscard]] static std::expected<MatchStart, DomainError> start(std::vector<PlayerId> seats,
                                                                      MatchSettings settings, RandomSource& random);

    // Forwards the action to the current round. When it ends the round, adds its points to the winner's
    // score and, if the match is decided too, appends MatchEnded. Rejected once the match is over.
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
    apply(const PlayerId& actor, const PlayerAction& action, RandomSource& random);

    // Takes a player out of the match for good (SPEC §5). The scores of the others are kept. With only two players
    // seated the match ends by forfeit: the player who stays wins and MatchEnded is the only event. UnknownPlayer if
    // the player is not seated, InvalidPhase once the match is over.
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> removePlayer(const PlayerId& player,
                                                                                    RandomSource& random);

    // Closes the UNO window on `player` (its time ran out, see Round::closeUnoWindow). Rejected once the match is over.
    [[nodiscard]] std::expected<void, DomainError> closeUnoWindow(const PlayerId& player);

    // Deals the next round, the dealer moving one seat clockwise. Only legal between two rounds of a
    // match that is not over (InvalidPhase otherwise). Returns the new round's starting events.
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> startNextRound(RandomSource& random);

    [[nodiscard]] const Round& round() const noexcept { return round_; }
    [[nodiscard]] std::uint32_t roundNumber() const noexcept { return progress_.roundNumber; }
    [[nodiscard]] const MatchSettings& settings() const noexcept { return settings_; }
    [[nodiscard]] const MatchProgress& progress() const noexcept { return progress_; }
    [[nodiscard]] std::expected<std::uint32_t, DomainError> score(const PlayerId& player) const;
    // Empty until the match is over.
    [[nodiscard]] const std::optional<PlayerId>& winner() const noexcept { return progress_.winner; }

    [[nodiscard]] bool operator==(const Match&) const = default;

private:
    Match(Round round, MatchSettings settings);

    Round round_;
    MatchSettings settings_;
    MatchProgress progress_;
};

struct MatchStart {
    Match match;
    std::vector<DomainEvent> events;
};

} // namespace uno::core
