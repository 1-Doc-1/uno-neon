import type { Emote } from '../protocol/generated/protocol';

export interface EmoteInfo {
  readonly id: Emote;
  /** Le pictogramme affiché : jamais du texte libre, seulement ces six-là (SPEC §8). */
  readonly glyph: string;
  /** Le nom lu par les lecteurs d'écran et affiché en info-bulle. */
  readonly label: string;
}

/** Exhaustif par construction : ajouter une émote au protocole oblige à la décrire ici. */
const BY_ID: Record<Emote, EmoteInfo> = {
  gg: { id: 'gg', glyph: '👏', label: 'Bien joué' },
  wow: { id: 'wow', glyph: '😮', label: 'Wow' },
  lol: { id: 'lol', glyph: '😂', label: 'Mort de rire' },
  ouch: { id: 'ouch', glyph: '😖', label: 'Aïe' },
  think: { id: 'think', glyph: '🤔', label: 'Hmm' },
  fire: { id: 'fire', glyph: '🔥', label: 'En feu' },
};

/** Les six réactions rapides, dans l'ordre du sélecteur. */
export const EMOTES: readonly EmoteInfo[] = [
  BY_ID.gg,
  BY_ID.wow,
  BY_ID.lol,
  BY_ID.ouch,
  BY_ID.think,
  BY_ID.fire,
];

export function emoteInfo(id: Emote): EmoteInfo {
  return BY_ID[id];
}
