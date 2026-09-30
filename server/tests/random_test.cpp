#include "uno/core/random_source.hpp"
#include "uno/testing/seeded_random_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <span>
#include <vector>

using uno::testing::SeededRandomSource;

namespace {

constexpr std::uint64_t kSeed = 42;

struct Draws {
    std::size_t count{};
    std::uint32_t upperExclusive{};
};

[[nodiscard]] std::vector<std::uint32_t> drawSequence(SeededRandomSource& random, Draws draws)
{
    std::vector<std::uint32_t> values(draws.count);
    std::ranges::generate(values, [&] { return random.uniform(draws.upperExclusive); });
    return values;
}

} // namespace

TEST_CASE("std::mt19937_64 output is fixed by the C++ standard", "[core][random]")
{
    // [rand.predef]: the 10000th consecutive invocation of a default-constructed
    // std::mt19937_64 shall produce 9981545732273789042 on every conforming implementation.
    // ADR 0008 relies on this guarantee for replays that are identical on every platform.
    // NOLINTNEXTLINE(bugprone-random-generator-seed,cert-msc32-c,cert-msc51-cpp): the default seed is the point
    std::mt19937_64 engine;
    engine.discard(9'999);

    REQUIRE(engine() == 9'981'545'732'273'789'042ULL);
}

TEST_CASE("SeededRandomSource always returns a value below the upper bound", "[core][random]")
{
    SeededRandomSource random{kSeed};

    for (const std::uint32_t upper : {2U, 3U, 7U, 108U, 1'000'000U, 0xFFFF'FFFFU}) {
        CAPTURE(upper);
        for (const auto value : drawSequence(random, {.count = 1'000, .upperExclusive = upper})) {
            REQUIRE(value < upper);
        }
    }
}

TEST_CASE("SeededRandomSource returns 0 when the upper bound is 1", "[core][random]")
{
    SeededRandomSource random{kSeed};

    REQUIRE(std::ranges::all_of(drawSequence(random, {.count = 100, .upperExclusive = 1}),
                                [](std::uint32_t value) { return value == 0; }));
}

TEST_CASE("SeededRandomSource with the same seed produces the same sequence", "[core][random]")
{
    SeededRandomSource first{kSeed};
    SeededRandomSource second{kSeed};
    SeededRandomSource other{kSeed + 1};

    const auto expected = drawSequence(first, {.count = 50, .upperExclusive = 108});
    REQUIRE(drawSequence(second, {.count = 50, .upperExclusive = 108}) == expected);
    REQUIRE(drawSequence(other, {.count = 50, .upperExclusive = 108}) != expected);
}

TEST_CASE("SeededRandomSource produces the same sequence on every platform", "[core][random]")
{
    // Reference values recorded once. The CI runs this test with MSVC, GCC and Clang:
    // a difference means a replay would diverge between platforms (ADR 0008).
    SeededRandomSource random{kSeed};

    REQUIRE(drawSequence(random, {.count = 10, .upperExclusive = 108}) ==
            std::vector<std::uint32_t>{66, 32, 94, 78, 77, 32, 4, 60, 82, 73});
}

TEST_CASE("SeededRandomSource has no visible bias on a small range", "[core][random]")
{
    constexpr std::uint32_t kFaces = 6;
    constexpr std::size_t kDraws = 60'000;
    constexpr std::size_t kExpected = kDraws / kFaces;
    constexpr std::size_t kTolerance = kExpected / 20; // ±5 %

    SeededRandomSource random{kSeed};
    std::array<std::size_t, kFaces> counts{};
    for (const auto value : drawSequence(random, {.count = kDraws, .upperExclusive = kFaces})) {
        ++counts.at(value);
    }

    for (const auto count : counts) {
        CAPTURE(count);
        REQUIRE(count > kExpected - kTolerance);
        REQUIRE(count < kExpected + kTolerance);
    }
}

TEST_CASE("Shuffle keeps exactly the same elements", "[core][random]")
{
    SeededRandomSource random{kSeed};
    std::vector<int> values(108);
    std::ranges::iota(values, 0);

    auto shuffled = values;
    uno::core::shuffle(std::span{shuffled}, random);

    REQUIRE(shuffled != values);
    std::ranges::sort(shuffled);
    REQUIRE(shuffled == values);
}

TEST_CASE("Shuffle is deterministic for a given seed", "[core][random]")
{
    const auto shuffleWithSeed = [](std::uint64_t seed) {
        SeededRandomSource random{seed};
        std::vector<int> values(20);
        std::ranges::iota(values, 0);
        uno::core::shuffle(std::span{values}, random);
        return values;
    };

    REQUIRE(shuffleWithSeed(kSeed) == shuffleWithSeed(kSeed));
    REQUIRE(shuffleWithSeed(kSeed) != shuffleWithSeed(kSeed + 1));
}

TEST_CASE("Shuffle reaches all six permutations of three elements with similar frequencies", "[core][random]")
{
    // An off-by-one in Fisher-Yates (drawing in [0, i) instead of [0, i]) gives Sattolo's
    // algorithm: only the 2 cyclic permutations out of 6 would ever appear.
    constexpr std::size_t kShuffles = 60'000;
    constexpr std::size_t kExpected = kShuffles / 6;
    constexpr std::size_t kTolerance = kExpected / 20; // ±5 %

    SeededRandomSource random{kSeed};
    std::map<std::array<int, 3>, std::size_t> counts;
    for (std::size_t i = 0; i < kShuffles; ++i) {
        std::array values{0, 1, 2};
        uno::core::shuffle(std::span{values}, random);
        ++counts[values];
    }

    REQUIRE(counts.size() == 6);
    for (const auto& [permutation, count] : counts) {
        CAPTURE(permutation, count);
        REQUIRE(count > kExpected - kTolerance);
        REQUIRE(count < kExpected + kTolerance);
    }
}

TEST_CASE("Shuffle of an empty or single-element range changes nothing", "[core][random]")
{
    SeededRandomSource random{kSeed};

    std::vector<int> empty;
    uno::core::shuffle(std::span{empty}, random);
    REQUIRE(empty.empty());

    std::array single{7};
    uno::core::shuffle(std::span{single}, random);
    REQUIRE(single == std::array{7});
}
