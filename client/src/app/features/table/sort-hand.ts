import type { Card, Color, Rank } from '../../protocol/generated/protocol';

const COLOR_ORDER: readonly Color[] = ['red', 'yellow', 'green', 'blue'];
/** Chiffres croissants, puis Passe, Inversion, +2 ; Jokers, +4 et +5 en dernier (ils n'ont pas de couleur). */
const RANK_ORDER: readonly Rank[] = [
  '0',
  '1',
  '2',
  '3',
  '4',
  '5',
  '6',
  '7',
  '8',
  '9',
  'skip',
  'reverse',
  'drawTwo',
  'wild',
  'wildDrawFour',
  'wildDrawFive',
];

const colorRank = (color: Color | null): number =>
  color === null ? COLOR_ORDER.length : COLOR_ORDER.indexOf(color);

/** L'ordre d'affichage de ma main : par couleur, puis par valeur. Pur affichage, le serveur ne le sait pas. */
export function sortHand(cards: readonly Card[]): Card[] {
  return [...cards].sort(
    (a, b) =>
      colorRank(a.color) - colorRank(b.color) ||
      RANK_ORDER.indexOf(a.rank) - RANK_ORDER.indexOf(b.rank) ||
      a.id - b.id,
  );
}
