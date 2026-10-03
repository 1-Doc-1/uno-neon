import type { Card, Color, Rank } from '../protocol/generated/protocol';

export const COLORS: readonly Color[] = ['red', 'yellow', 'green', 'blue'];

export const COLOR_NAME: Record<Color, string> = {
  red: 'Rouge',
  yellow: 'Jaune',
  green: 'Vert',
  blue: 'Bleu',
};

export const SHAPE_NAME: Record<Color, string> = {
  red: 'triangle',
  yellow: 'cercle',
  green: 'carré',
  blue: 'losange',
};

const RANK_NAME: Record<Rank, string> = {
  '0': '0',
  '1': '1',
  '2': '2',
  '3': '3',
  '4': '4',
  '5': '5',
  '6': '6',
  '7': '7',
  '8': '8',
  '9': '9',
  skip: 'Passe-tour',
  reverse: 'Inversion',
  drawTwo: 'Plus deux',
  wild: 'Joker',
  wildDrawFour: 'Joker plus quatre',
};

export function tintClass(color: Color | null): string {
  return color === null ? 'tint-wild' : `tint-${color}`;
}

export function cardLabel(card: Card): string {
  const rank = RANK_NAME[card.rank];
  return card.color === null ? rank : `${rank} ${COLOR_NAME[card.color].toLowerCase()}`;
}

/** Caractère de la forme associée à chaque couleur, pour les textes (« Bleu ◆ »). */
export const SHAPE_CHAR: Record<Color, string> = {
  red: '▲',
  yellow: '●',
  green: '■',
  blue: '◆',
};
