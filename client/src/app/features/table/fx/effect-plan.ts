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
  /**
   * Un effet qui vise UN joueur (`victimId`) : en grand au centre chez lui seulement, sur son siège chez les autres.
   * Il prend la couleur de la carte jouée (`null` : une carte noire, donc blanc).
   */
  | {
      readonly kind: 'bigText';
      readonly text: '+2' | '+4';
      readonly amount: number;
      readonly victimId: string | null;
      readonly color: Color | null;
    }
  | { readonly kind: 'skip'; readonly playerId: string }
  | { readonly kind: 'reverse'; readonly direction: Direction }
  | { readonly kind: 'wheel'; readonly color: Color }
  | { readonly kind: 'turn'; readonly from: string | null; readonly to: string }
  | { readonly kind: 'uno'; readonly playerId: string }
  | { readonly kind: 'caught'; readonly targetId: string; readonly amount: number }
  | {
      readonly kind: 'challenge';
      readonly succeeded: boolean;
      readonly penalizedId: string;
      readonly amount: number;
    }
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

/**
 * Durées d'une étape, à vitesse normale. Une pioche est rythmée par le serveur (`drawStepMs`, ADR 0026) : une carte
 * toutes les `drawStepMs`, et l'étape dure autant que le serveur attend avant son prochain coup forcé.
 */
export function timingOf(spec: EffectSpec, reduced: boolean, drawStepMs: number): Timing {
  if (spec.kind === 'draw') {
    // Le rythme est celui du serveur, même en mouvement réduit : seul le vol devient un fondu
    return {
      stepMs: spec.count * drawStepMs,
      visibleMs: (spec.count - 1) * drawStepMs + (reduced ? REDUCED_MS : MOTION_MS.drawFlight),
    };
  }
  if (reduced) {
    return { stepMs: spec.kind === 'turn' ? 0 : REDUCED_MS, visibleMs: REDUCED_MS };
  }
  return FIXED_TIMING[spec.kind];
}

/** Le joueur visé par une carte à pénalité : le premier qui pioche, passe ou reçoit le tour après elle dans le lot. */
function victimAfter(events: readonly ClientEvent[], playedAt: number): string | null {
  for (const event of events.slice(playedAt + 1)) {
    if (
      event.kind === 'cardsDrawn' ||
      event.kind === 'playerSkipped' ||
      event.kind === 'turnChanged'
    ) {
      return event.playerId;
    }
  }
  return null;
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

  for (const [index, event] of events.entries()) {
    switch (event.kind) {
      case 'cardPlayed': {
        specs.push({ kind: 'play', playerId: event.playerId, card: event.card });
        if (event.card.rank === 'drawTwo' || event.card.rank === 'wildDrawFour') {
          const two = event.card.rank === 'drawTwo';
          specs.push({
            kind: 'bigText',
            text: two ? '+2' : '+4',
            amount: two ? 2 : 4,
            victimId: victimAfter(events, index),
            color: event.card.color,
          });
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
          // Une pioche de plusieurs cartes arrive en un événement par carte : elle ne fait qu'une étape
          const previous = specs.at(-1);
          if (
            previous?.kind === 'draw' &&
            previous.playerId === event.playerId &&
            !penaltyPending
          ) {
            specs[specs.length - 1] = {
              ...previous,
              count: previous.count + event.count,
              cards:
                previous.cards && event.cards ? [...previous.cards, ...event.cards] : undefined,
            };
          } else {
            specs.push({
              kind: 'draw',
              playerId: event.playerId,
              count: event.count,
              cards: event.cards,
              penalty: penaltyPending,
            });
          }
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
        specs.push({ kind: 'caught', targetId: event.targetId, amount: event.penaltyAmount });
        penaltyPending = true;
        break;
      case 'challengeResolved':
        specs.push({
          kind: 'challenge',
          succeeded: event.wasBluff,
          penalizedId: event.penalizedPlayerId,
          amount: event.penaltyAmount,
        });
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
