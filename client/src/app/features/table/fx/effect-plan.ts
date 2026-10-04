import type { Card, ClientEvent, Color, Direction } from '../../../protocol/generated/protocol';
import { MOTION_MS } from '../../../ui/motion';

/** Ce qu'on montre : une description pure, sans position ni durée (la couche d'effets la traduit en pixels). */
export type EffectSpec =
  | { readonly kind: 'play'; readonly playerId: string; readonly card: Card }
  | {
      readonly kind: 'draw';
      readonly playerId: string;
      readonly count: number;
      /** Les cartes piochées, connues du seul joueur qui pioche (jamais celles d'un adversaire). */
      readonly cards: readonly Card[] | undefined;
      /** Pioche infligée (+2, +4, contestation, contre-UNO) : le siège tremble et clignote. */
      readonly penalty: boolean;
    }
  | { readonly kind: 'bigText'; readonly text: '+2' | '+4' }
  | { readonly kind: 'skip'; readonly playerId: string }
  | { readonly kind: 'reverse'; readonly direction: Direction }
  | { readonly kind: 'wheel'; readonly color: Color }
  | { readonly kind: 'turn'; readonly from: string | null; readonly to: string }
  | { readonly kind: 'uno'; readonly playerId: string }
  | { readonly kind: 'caught'; readonly targetId: string }
  | { readonly kind: 'challenge'; readonly succeeded: boolean }
  | { readonly kind: 'spotlight'; readonly playerId: string };

export type EffectKind = EffectSpec['kind'];

/** Un seul effet plein écran à la fois : ces effets occupent le centre de la table, leur durée visible est leur durée. */
const FULLSCREEN: ReadonlySet<EffectKind> = new Set([
  'bigText',
  'reverse',
  'wheel',
  'challenge',
  'spotlight',
]);

export function isFullscreen(kind: EffectKind): boolean {
  return FULLSCREEN.has(kind);
}

/** Temps (ms) avant l'effet suivant, et temps pendant lequel celui-ci reste visible (les petits effets se chevauchent). */
interface Timing {
  readonly stepMs: number;
  readonly visibleMs: number;
}

export const MAX_FLYING_CARDS = 6;
export const REDUCED_MS = MOTION_MS.reduced;

// Les durées viennent toutes de `ui/motion.ts`. Une carte posée reste visible un instant (`playRest`) avant l'action suivante.
const FIXED_TIMING: Record<Exclude<EffectKind, 'draw'>, Timing> = {
  play: { stepMs: MOTION_MS.playFlight + MOTION_MS.playRest, visibleMs: MOTION_MS.playFlight },
  bigText: { stepMs: MOTION_MS.bigText, visibleMs: MOTION_MS.bigText },
  skip: { stepMs: MOTION_MS.specialGap, visibleMs: MOTION_MS.skip },
  reverse: { stepMs: MOTION_MS.reverse, visibleMs: MOTION_MS.reverse },
  wheel: { stepMs: MOTION_MS.wheel, visibleMs: MOTION_MS.wheel },
  turn: { stepMs: 90, visibleMs: MOTION_MS.turn },
  uno: { stepMs: MOTION_MS.specialGap, visibleMs: MOTION_MS.uno },
  caught: { stepMs: MOTION_MS.specialGap, visibleMs: MOTION_MS.caught },
  challenge: { stepMs: MOTION_MS.challenge, visibleMs: MOTION_MS.challenge },
  spotlight: { stepMs: MOTION_MS.spotlight, visibleMs: MOTION_MS.spotlight },
};

export function flyingCards(count: number): number {
  return Math.min(count, MAX_FLYING_CARDS);
}

export function timingOf(spec: EffectSpec, reduced: boolean): Timing {
  if (reduced) {
    return { stepMs: spec.kind === 'turn' ? 0 : REDUCED_MS, visibleMs: REDUCED_MS };
  }
  if (spec.kind === 'draw') {
    const lastDeparture = (flyingCards(spec.count) - 1) * MOTION_MS.drawStagger;
    // Une seule carte : la suivante (autre événement) part `drawStagger` plus tard, d'où l'effet de cartes l'une après l'autre
    return {
      stepMs: MOTION_MS.drawStagger + lastDeparture,
      visibleMs: MOTION_MS.drawFlight + lastDeparture,
    };
  }
  return FIXED_TIMING[spec.kind];
}

export interface PlanState {
  readonly turnPlayerId: string | null;
  /** Une pioche infligée est attendue : la prochaine pioche est une pénalité, pas un choix du joueur. */
  readonly penaltyPending: boolean;
}

export const INITIAL_PLAN_STATE: PlanState = { turnPlayerId: null, penaltyPending: false };

/**
 * Traduit un lot d'événements du serveur en effets, dans l'ordre. L'état d'un lot à l'autre (qui jouait, une pénalité
 * attendue) est rendu à l'appelant : la fonction est pure. Les animations ne dépendent que des événements, jamais d'une
 * comparaison de vues.
 */
export function planEffects(
  events: readonly ClientEvent[],
  initial: PlanState,
): { readonly specs: readonly EffectSpec[]; readonly state: PlanState } {
  const specs: EffectSpec[] = [];
  let { turnPlayerId, penaltyPending } = initial;

  for (const event of events) {
    switch (event.kind) {
      case 'cardPlayed': {
        specs.push({ kind: 'play', playerId: event.playerId, card: event.card });
        if (event.card.rank === 'drawTwo' || event.card.rank === 'wildDrawFour') {
          specs.push({ kind: 'bigText', text: event.card.rank === 'drawTwo' ? '+2' : '+4' });
          penaltyPending = true;
        }
        // Le serveur annonce aussi le choix dans un `colorChosen` : une seule roue par Joker
        const announced = events.some(
          (other) => other.kind === 'colorChosen' && other.playerId === event.playerId,
        );
        if (event.card.color === null && event.chosenColor !== null && !announced) {
          specs.push({ kind: 'wheel', color: event.chosenColor });
        }
        break;
      }
      case 'colorChosen':
        specs.push({ kind: 'wheel', color: event.color });
        break;
      case 'cardsDrawn':
        if (event.count > 0) {
          specs.push({
            kind: 'draw',
            playerId: event.playerId,
            count: event.count,
            cards: event.cards,
            penalty: penaltyPending,
          });
        }
        penaltyPending = false;
        break;
      case 'playerSkipped':
        specs.push({ kind: 'skip', playerId: event.playerId });
        break;
      case 'directionChanged':
        specs.push({ kind: 'reverse', direction: event.direction });
        break;
      case 'turnChanged':
        if (event.playerId !== turnPlayerId) {
          specs.push({ kind: 'turn', from: turnPlayerId, to: event.playerId });
          turnPlayerId = event.playerId;
        }
        break;
      case 'unoCalled':
        specs.push({ kind: 'uno', playerId: event.playerId });
        break;
      case 'unoCaught':
        specs.push({ kind: 'caught', targetId: event.targetId });
        penaltyPending = true;
        break;
      case 'challengeResolved':
        specs.push({ kind: 'challenge', succeeded: event.wasBluff });
        penaltyPending = true;
        break;
      case 'roundEnded':
        specs.push({ kind: 'spotlight', playerId: event.winnerId });
        break;
      default:
        break;
    }
  }
  return { specs, state: { turnPlayerId, penaltyPending } };
}
