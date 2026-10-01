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
    return settings;
}

} // namespace uno::app
