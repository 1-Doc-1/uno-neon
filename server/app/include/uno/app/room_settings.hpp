#pragma once

#include "uno/core/deck.hpp"
#include "uno/core/draw_amount.hpp"
#include "uno/core/draw_rule.hpp"
#include "uno/core/match.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace uno::app {

enum class StackingMode : std::uint8_t { Off, SameType, Mixed };
enum class WildDrawFourMode : std::uint8_t { OfficialChallenge, Strict };
enum class TurnTimerSeconds : std::uint8_t { Off = 0, Fifteen = 15, Thirty = 30, Sixty = 60 };

inline constexpr std::uint8_t kMinRoomPlayers = 2;
inline constexpr std::uint8_t kMaxRoomPlayers = 10;

// House rules chosen by the host (SPEC §4, protocol `RoomSettings`).
struct RoomSettings {
    StackingMode stacking{StackingMode::Off};
    bool jumpIn{false};
    bool sevenZero{false};
    core::DrawAmount drawAmount{core::DrawAmount::UntilPlayable}; // ADR 0024
    WildDrawFourMode wildDrawFourMode{WildDrawFourMode::OfficialChallenge};
    TurnTimerSeconds turnTimer{TurnTimerSeconds::Thirty};
    core::MatchLength matchLength{core::MatchLength::To500};
    std::uint8_t maxPlayers{6};
    core::DrawRule drawRule{core::DrawRule::Guided}; // ADR 0017
    bool declareUnoToWin{false};                     // ADR 0019: the last card needs an announcement
    // ADR 0028: how many Draw Two, Wild Draw Four and Wild Draw Five the deck holds (x1, x2, x3 or x5)
    core::CardMultiplier drawTwoMultiplier{core::CardMultiplier::One};
    core::CardMultiplier wildDrawFourMultiplier{core::CardMultiplier::One};
    core::CardMultiplier wildDrawFiveMultiplier{core::CardMultiplier::One};

    [[nodiscard]] core::DeckSettings deck() const noexcept
    {
        return {.drawTwo = drawTwoMultiplier,
                .wildDrawFour = wildDrawFourMultiplier,
                .wildDrawFive = wildDrawFiveMultiplier};
    }

    [[nodiscard]] bool operator==(const RoomSettings&) const = default;
};

// Partial settings: only the fields to change (protocol `RoomSettingsPatch`).
struct RoomSettingsPatch {
    std::optional<StackingMode> stacking;
    std::optional<bool> jumpIn;
    std::optional<bool> sevenZero;
    std::optional<core::DrawAmount> drawAmount;
    std::optional<WildDrawFourMode> wildDrawFourMode;
    std::optional<TurnTimerSeconds> turnTimer;
    std::optional<core::MatchLength> matchLength;
    std::optional<std::uint8_t> maxPlayers;
    std::optional<core::DrawRule> drawRule;
    std::optional<bool> declareUnoToWin;
    std::optional<core::CardMultiplier> drawTwoMultiplier;
    std::optional<core::CardMultiplier> wildDrawFourMultiplier;
    std::optional<core::CardMultiplier> wildDrawFiveMultiplier;

    [[nodiscard]] bool operator==(const RoomSettingsPatch&) const = default;
};

// Why these settings cannot be used by a room of `memberCount` members, or nothing when they can: a
// maxPlayers below the number of members, or a house rule the engine does not implement yet (step 1.6).
[[nodiscard]] std::optional<std::string> settingsProblem(const RoomSettings& settings, std::size_t memberCount);

[[nodiscard]] RoomSettings applyPatch(RoomSettings settings, const RoomSettingsPatch& patch) noexcept;

} // namespace uno::app
