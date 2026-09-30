#include "uno/core/domain_error.hpp"
#include "uno/core/player_id.hpp"
#include "uno/core/turn_order.hpp"
#include "uno/testing/fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <vector>

using uno::core::Direction;
using uno::core::DomainError;
using uno::core::PlayerId;
using uno::core::TurnOrder;
using uno::testing::player;
using uno::testing::players;

namespace {

[[nodiscard]] TurnOrder turnOrder(std::size_t playerCount, std::size_t dealer)
{
    auto order = TurnOrder::startingLeftOf(players(playerCount), player(dealer));
    REQUIRE(order.has_value());
    return *order;
}

// Current players seen while advancing `turns` times, starting with the current one.
[[nodiscard]] std::vector<PlayerId> playSequence(TurnOrder order, std::size_t turns)
{
    std::vector<PlayerId> sequence{order.current()};
    for (std::size_t turn = 0; turn < turns; ++turn) {
        order.advance();
        sequence.push_back(order.current());
    }
    return sequence;
}

} // namespace

TEST_CASE("Turn order rejects fewer than 2 players", "[core][turn]")
{
    const auto playerCount = GENERATE(std::size_t{0}, std::size_t{1});
    const auto order = TurnOrder::startingLeftOf(players(playerCount), player(0));

    REQUIRE(order.error() == DomainError::NotEnoughPlayers);
}

TEST_CASE("Turn order rejects more than 10 players", "[core][turn]")
{
    STATIC_REQUIRE(uno::core::kMinPlayers == 2);
    STATIC_REQUIRE(uno::core::kMaxPlayers == 10);
    REQUIRE(TurnOrder::startingLeftOf(players(10), player(0)).has_value());

    const auto order = TurnOrder::startingLeftOf(players(11), player(0));

    REQUIRE(order.error() == DomainError::TooManyPlayers);
}

TEST_CASE("Turn order rejects duplicate player ids", "[core][turn]")
{
    const auto order = TurnOrder::startingLeftOf({player(0), player(1), player(0)}, player(1));

    REQUIRE(order.error() == DomainError::DuplicatePlayer);
}

TEST_CASE("Turn order rejects a dealer who is not seated", "[core][turn]")
{
    const auto order = TurnOrder::startingLeftOf(players(3), player(7));

    REQUIRE(order.error() == DomainError::DealerNotSeated);
}

TEST_CASE("First player sits to the left of the dealer", "[core][turn]")
{
    const auto order = turnOrder(4, 1);

    REQUIRE(order.current() == player(2));
    REQUIRE(order.direction() == Direction::Clockwise);
}

TEST_CASE("First player wraps around when the dealer has the last seat", "[core][turn]")
{
    REQUIRE(turnOrder(4, 3).current() == player(0));
}

TEST_CASE("Play goes clockwise in increasing seat order by default", "[core][turn]")
{
    REQUIRE(playSequence(turnOrder(4, 3), 3) == std::vector{player(0), player(1), player(2), player(3)});
}

TEST_CASE("Advancing past the last seat wraps to the first seat", "[core][turn]")
{
    REQUIRE(playSequence(turnOrder(3, 1), 2) == std::vector{player(2), player(0), player(1)});
}

TEST_CASE("Reversing makes play go counter-clockwise", "[core][turn]")
{
    auto order = turnOrder(4, 0);
    order.reverse();

    REQUIRE(order.direction() == Direction::CounterClockwise);
    REQUIRE(playSequence(order, 4) == std::vector{player(1), player(0), player(3), player(2), player(1)});
}

TEST_CASE("Reversing twice restores clockwise play", "[core][turn]")
{
    auto order = turnOrder(4, 0);
    order.reverse();
    order.reverse();

    REQUIRE(order.direction() == Direction::Clockwise);
    REQUIRE(playSequence(order, 2) == std::vector{player(1), player(2), player(3)});
}

TEST_CASE("Next player is peeked without changing the current player", "[core][turn]")
{
    auto order = turnOrder(3, 0);
    REQUIRE(order.next() == player(2));
    REQUIRE(order.current() == player(1));

    order.reverse();
    REQUIRE(order.next() == player(0));
    REQUIRE(order.current() == player(1));
}

TEST_CASE("After reversing, advancing from the player left of the dealer gives the turn back to the dealer",
          "[core][turn]")
{
    auto order = turnOrder(5, 2);
    order.reverse();
    order.advance();

    REQUIRE(order.current() == player(2));
    REQUIRE(order.next() == player(1));
}

TEST_CASE("Two-player turn order alternates in both directions", "[core][turn]")
{
    auto order = turnOrder(2, 0);
    REQUIRE(playSequence(order, 3) == std::vector{player(1), player(0), player(1), player(0)});

    order.reverse();
    REQUIRE(playSequence(order, 3) == std::vector{player(1), player(0), player(1), player(0)});
}

TEST_CASE("Seat of a player is their index in the seating order", "[core][turn]")
{
    const auto order = turnOrder(3, 0);

    REQUIRE(order.seatOf(player(2)) == std::size_t{2});
    REQUIRE_FALSE(order.seatOf(player(5)).has_value());
}
