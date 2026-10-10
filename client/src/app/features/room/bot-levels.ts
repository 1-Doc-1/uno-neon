import type { BotLevel } from '../../protocol/generated/protocol';
import type { SegmentOption } from '../../ui/segmented';

/** Les niveaux d'un bot, tels que le salon et l'écran « Jouer contre des bots » les proposent. */
export const BOT_LEVELS: readonly SegmentOption<BotLevel>[] = [
  { value: 'easy', label: 'Facile' },
  { value: 'normal', label: 'Normal' },
];
