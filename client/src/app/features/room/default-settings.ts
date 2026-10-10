import type { RoomSettings } from '../../protocol/generated/protocol';

/**
 * Les réglages d'un salon neuf, tels que le serveur les donne (`RoomSettings` du serveur). L'écran « Jouer contre des
 * bots » les affiche avant que le salon existe, puis les envoie avec la demande.
 */
export const DEFAULT_SETTINGS: RoomSettings = {
  stacking: 'official',
  jumpIn: false,
  sevenZero: false,
  drawAmount: 'untilPlayable',
  wildDrawFourMode: 'officialChallenge',
  turnTimerSeconds: 30,
  matchLength: 'to500',
  maxPlayers: 6,
  drawRule: 'guided',
  declareUnoToWin: false,
  drawTwoMultiplier: 1,
  wildDrawFourMultiplier: 1,
  wildDrawFiveMultiplier: 1,
};
