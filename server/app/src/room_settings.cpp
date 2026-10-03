#include "uno/app/room_settings.hpp"

namespace uno::app {

RoomSettings applyPatch(RoomSettings settings, const RoomSettingsPatch& patch) noexcept
{
    if (patch.stacking) {
        settings.stacking = *patch.stacking;
    }
    if (patch.jumpIn) {
        settings.jumpIn = *patch.jumpIn;
    }
    if (patch.sevenZero) {
        settings.sevenZero = *patch.sevenZero;
    }
    if (patch.drawUntilPlayable) {
        settings.drawUntilPlayable = *patch.drawUntilPlayable;
    }
    if (patch.wildDrawFourMode) {
        settings.wildDrawFourMode = *patch.wildDrawFourMode;
    }
    if (patch.turnTimer) {
        settings.turnTimer = *patch.turnTimer;
    }
    if (patch.matchLength) {
        settings.matchLength = *patch.matchLength;
    }
    if (patch.maxPlayers) {
        settings.maxPlayers = *patch.maxPlayers;
    }
    if (patch.drawRule) {
        settings.drawRule = *patch.drawRule;
    }
    return settings;
}

std::optional<std::string> settingsProblem(const RoomSettings& settings, std::size_t memberCount)
{
    if (settings.maxPlayers < memberCount) {
        return "maxPlayers is below the number of players in the room";
    }
    const RoomSettings defaults;
    if (settings.stacking != defaults.stacking || settings.jumpIn || settings.sevenZero || settings.drawUntilPlayable ||
        settings.wildDrawFourMode != defaults.wildDrawFourMode) {
        return "house rules are not available yet";
    }
    return std::nullopt;
}

} // namespace uno::app
