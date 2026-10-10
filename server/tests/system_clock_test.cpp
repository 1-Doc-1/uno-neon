#include "uno/net/system_clock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("The server clock starts at the wall clock and never goes back", "[net][clock]")
{
    const auto before = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    const uno::net::SystemClock clock;

    const std::int64_t first = clock.nowMillis();
    std::this_thread::sleep_for(30ms);
    const std::int64_t second = clock.nowMillis();

    REQUIRE(first >= before);
    REQUIRE(first - before < 1000);
    REQUIRE(second >= first + 25); // it advances with real time
}
