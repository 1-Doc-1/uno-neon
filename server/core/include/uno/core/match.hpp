#pragma once

#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/round.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace uno::core {

inline constexpr std::uint32_t kDefaultTargetScore = 500;

struct MatchSettings {
    // Score a player must reach to win the match (SPEC §4); empty means a single round.
    std::optional<std::uint32_t> targetScore{kDefaultTargetScore};

    [[nodiscard]] bool operator==(const MatchSettings&) const = default;
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

    // Deals the next round, the dealer moving one seat clockwise. Only legal between two rounds of a
    // match that is not over (InvalidPhase otherwise). Returns the new round's starting events.
    [[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError> startNextRound(RandomSource& random);

    [[nodiscard]] const Round& round() const noexcept { return round_; }
    [[nodiscard]] std::uint32_t roundNumber() const noexcept { return roundNumber_; }
    [[nodiscard]] const MatchSettings& settings() const noexcept { return settings_; }
    [[nodiscard]] std::expected<std::uint32_t, DomainError> score(const PlayerId& player) const;
    // Empty until the match is over.
    [[nodiscard]] const std::optional<PlayerId>& winner() const noexcept { return winner_; }

    [[nodiscard]] bool operator==(const Match&) const = default;

private:
    Match(Round round, MatchSettings settings);

    Round round_;
    MatchSettings settings_;
    std::vector<std::uint32_t> scores_; // indexed by seat
    std::uint32_t roundNumber_{1};
    std::optional<PlayerId> winner_;
};

struct MatchStart {
    Match match;
    std::vector<DomainEvent> events;
};

} // namespace uno::core
