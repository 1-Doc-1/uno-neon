import type { Card, ClientEvent } from '../../../protocol/generated/protocol';

/** Combien de cartes on montre en pile désordonnée sur la défausse (la carte du dessus comprise). */
export const VISIBLE_DISCARDS = 5;
const MAX_TILT_DEGREES = 11;
const MAX_OFFSET_PX = 7;

export interface Scatter {
  readonly rotation: number;
  readonly dx: number;
  readonly dy: number;
}

export interface PileCard extends Scatter {
  readonly card: Card;
}

/** Mélange d'entiers (Knuth) : le même identifiant donne toujours le même nombre, sur tous les navigateurs. */
function mix(seed: number, salt: number): number {
  let h = Math.imul((seed + 1) ^ Math.imul(salt, 0x9e3779b1), 2654435761) >>> 0;
  h ^= h >>> 15;
  h = Math.imul(h, 2246822519) >>> 0;
  return (h ^ (h >>> 13)) >>> 0;
}

/** Valeur dans [-1, 1] dérivée de l'identifiant de la carte : la même à chaque rendu. */
function unit(cardId: number, salt: number): number {
  return (mix(cardId, salt) % 2001) / 1000 - 1;
}

/** La rotation et le décalage « désordonnés » d'une carte de la défausse, déterministes d'après son identifiant. */
export function scatterOf(cardId: number): Scatter {
  return {
    rotation: unit(cardId, 1) * MAX_TILT_DEGREES,
    dx: unit(cardId, 2) * MAX_OFFSET_PX,
    dy: unit(cardId, 3) * MAX_OFFSET_PX,
  };
}

/**
 * Les cartes posées depuis le début de la manche en cours, d'après les événements `cardPlayed` (la vue ne connaît que
 * la carte du dessus). Une nouvelle manche ou un remélange repartent de zéro.
 */
export function playedCards(events: readonly ClientEvent[]): readonly Card[] {
  const played: Card[] = [];
  for (const event of events) {
    if (event.kind === 'roundStarted' || event.kind === 'deckReshuffled') {
      played.length = 0;
    } else if (event.kind === 'cardPlayed') {
      played.push(event.card);
    }
  }
  return played;
}

/** La pile à dessiner, de la plus ancienne à la carte du dessus (toujours celle de la vue). */
export function discardStack(played: readonly Card[], top: Card): readonly PileCard[] {
  const history = played.at(-1)?.id === top.id ? played : [top];
  return history.slice(-VISIBLE_DISCARDS).map((card) => ({ card, ...scatterOf(card.id) }));
}
