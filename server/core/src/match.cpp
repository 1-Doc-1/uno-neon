#include "uno/core/match.hpp"

#include "uno/core/deck.hpp"
#include "uno/core/domain_error.hpp"
#include "uno/core/domain_event.hpp"
#include "uno/core/player_action.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/random_source.hpp"
#include "uno/core/round.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/core/turn_phase.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace uno::core {

namespace {

[[nodiscard]] std::expected<RoundStart, DomainError> dealRound(std::vector<PlayerId> seats, PlayerId dealer,
                                                               RandomSource& random)
{
    auto deck = createStandardDeck(random);
    shuffle(std::span{deck}, random);
    return Round::start({.seats = std::move(seats), .dealer = std::move(dealer), .deck = std::move(deck)}, random);
}

} // namespace

Match::Match(Round round, MatchSettings settings)
    : round_{std::move(round)}, settings_{settings}, scores_(round_.seats().size(), 0)
{
}

std::expected<MatchStart, DomainError> Match::start(std::vector<PlayerId> seats, MatchSettings settings,
                                                    RandomSource& random)
{
    if (seats.size() < kMinPlayers) {
        return std::unexpected{DomainError::NotEnoughPlayers};
    }
    auto firstDealer = seats.at(random.uniform(static_cast<std::uint32_t>(seats.size())));
    auto started = dealRound(std::move(seats), std::move(firstDealer), random);
    if (!started) {
        return std::unexpected{started.error()};
    }
    return MatchStart{.match = Match{std::move(started->round), settings}, .events = std::move(started->events)};
}

std::expected<std::vector<DomainEvent>, DomainError> Match::apply(const PlayerId& actor, const PlayerAction& action,
                                                                  RandomSource& random)
{
    if (winner_.has_value()) {
        return std::unexpected{DomainError::InvalidPhase};
    }
    auto events = round_.apply(actor, action, random);
    // A round that is over rejects every action, so finding it over here means this very action ended it.
    const auto* over = std::get_if<RoundOver>(&round_.phase());
    if (!events || over == nullptr) {
        return events;
    }

    const auto seats = round_.seats();
    auto& winnerScore = scores_.at(static_cast<std::size_t>(std::ranges::find(seats, over->winner) - seats.begin()));
    winnerScore += over->points;
    if (!settings_.targetScore.has_value() || winnerScore >= *settings_.targetScore) {
        winner_ = over->winner;
        events->emplace_back(MatchEnded{.winner = over->winner});
    }
    return events;
}

std::expected<std::vector<DomainEvent>, DomainError> Match::startNextRound(RandomSource& random)
{
    if (winner_.has_value() || !std::holds_alternative<RoundOver>(round_.phase())) {
        return std::unexpected{DomainError::InvalidPhase};
    }
    std::vector<PlayerId> seats{round_.seats().begin(), round_.seats().end()};
    const auto dealerSeat = static_cast<std::size_t>(std::ranges::find(seats, round_.dealer()) - seats.begin());
    auto nextDealer = seats.at((dealerSeat + 1) % seats.size());
    auto started = dealRound(std::move(seats), std::move(nextDealer), random);
    if (!started) {
        return std::unexpected{started.error()};
    }
    round_ = std::move(started->round);
    ++roundNumber_;
    return std::move(started->events);
}

std::expected<std::uint32_t, DomainError> Match::score(const PlayerId& player) const
{
    const auto seats = round_.seats();
    const auto found = std::ranges::find(seats, player);
    if (found == seats.end()) {
        return std::unexpected{DomainError::UnknownPlayer};
    }
    return scores_.at(static_cast<std::size_t>(std::distance(seats.begin(), found)));
}

} // namespace uno::core
