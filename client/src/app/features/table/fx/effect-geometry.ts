import type { Card, Color, Direction } from '../../../protocol/generated/protocol';
import type { ActiveEffect } from './animation-director';
import { scatterOf } from './discard-pile';

/** Une boîte sur l'écran, par son centre, en pixels relatifs à la couche d'effets. */
export interface Box {
  readonly cx: number;
  readonly cy: number;
  readonly w: number;
  readonly h: number;
}

/** Comment une carte en vol se montre : de face, de dos puis retournée en arrivant, ou de dos. */
export type FlightLook = 'face' | 'back-to-face' | 'back';

export interface Flight {
  readonly from: Box;
  readonly to: Box;
  readonly fromTilt: number;
  readonly toTilt: number;
  readonly delayMs: number;
  readonly durationMs: number;
  readonly card: Card | null;
  readonly look: FlightLook;
  /** La carte se pose sur la défausse : elle y rebondit légèrement. */
  readonly settle: boolean;
}

interface Base {
  readonly id: number;
  readonly durationMs: number;
  readonly reduced: boolean;
}

export type LabelVariant = 'uno' | 'caught' | 'challenge' | 'seat';

/** La couleur d'un effet : celle de la carte jouée, ou le blanc d'une carte noire. */
export type Tone = Color | 'white';

export type Placed =
  | (Base & { readonly kind: 'flights'; readonly flights: readonly Flight[] })
  | (Base & {
      readonly kind: 'bigText';
      readonly text: string;
      /** La ligne en dessous, à la deuxième personne (« Tu pioches 2 »). */
      readonly caption: string | null;
      readonly tone: Tone;
      readonly at: Box;
    })
  | (Base & {
      readonly kind: 'label';
      readonly variant: LabelVariant;
      readonly text: string;
      readonly tone: Tone | null;
      /** Au centre de la table (effet personnel chez la cible) ou au-dessus d'un siège (chez les autres). */
      readonly where: 'center' | 'seat';
      readonly at: Box;
    })
  | (Base & {
      readonly kind: 'skip';
      readonly caption: string;
      readonly where: 'center' | 'seat';
      readonly at: Box;
    })
  | (Base & { readonly kind: 'wheel'; readonly color: Color; readonly at: Box })
  | (Base & { readonly kind: 'orb'; readonly from: Box; readonly to: Box })
  | (Base & { readonly kind: 'spotlight'; readonly at: Box })
  | (Base & { readonly kind: 'reverse'; readonly direction: Direction; readonly at: Box });

/** Ce que la géométrie demande à l'écran : des boîtes repérées par des ancres, relatives à la couche. */
export interface Anchors {
  readonly meId: string;
  /** Le pseudo d'un joueur, pour les effets vus par les autres (« Zoé pioche 2 »). */
  readonly nameOf: (playerId: string) => string;
  readonly box: (anchor: string) => Box | null;
  /** La dernière position connue d'une carte de ma main, même si elle vient de la quitter. */
  readonly handCard: (cardId: number) => Box | null;
  readonly layer: Box;
}

/** Une carte qui part d'un siège ou y arrive est dessinée à cette échelle de la défausse. */
const SEAT_CARD_RATIO = 0.4;
const HAND_FALLBACK_RATIO = 0.8;

const seatOf = (anchors: Anchors, playerId: string): Box | null => anchors.box(`seat:${playerId}`);

/** Une carte de la taille de `model`, centrée sur `where`. */
function sizedLike(where: Box, model: Box, ratio: number): Box {
  return { cx: where.cx, cy: where.cy, w: model.w * ratio, h: model.h * ratio };
}

function placePlay(
  effect: ActiveEffect,
  card: Card,
  playerId: string,
  anchors: Anchors,
): Placed | null {
  const discard = anchors.box('discard');
  if (!discard) {
    return null;
  }
  const mine = playerId === anchors.meId;
  const { layer } = anchors;
  const source = mine
    ? (anchors.handCard(card.id) ??
      sizedLike(
        { cx: layer.cx, cy: layer.cy + layer.h / 2 - discard.h / 2, w: 0, h: 0 },
        discard,
        HAND_FALLBACK_RATIO,
      ))
    : sizedLike(seatOf(anchors, playerId) ?? discard, discard, SEAT_CARD_RATIO);
  const { rotation: tilt, dx, dy } = scatterOf(card.id);
  return {
    id: effect.id,
    durationMs: effect.durationMs,
    reduced: effect.reduced,
    kind: 'flights',
    flights: [
      {
        from: source,
        to: { ...discard, cx: discard.cx + dx, cy: discard.cy + dy },
        fromTilt: -tilt,
        toTilt: tilt,
        delayMs: 0,
        durationMs: effect.durationMs,
        card,
        look: mine ? 'face' : 'back-to-face',
        settle: true,
      },
    ],
  };
}

/** Où atterrit une carte de ma main dont la place n'est pas encore connue : une carte de la taille du paquet, au milieu de la main (jamais la boîte entière de la main). */
function handFallback(anchors: Anchors, deck: Box): Box {
  const hand = anchors.box('hand');
  return hand ? sizedLike(hand, deck, HAND_FALLBACK_RATIO) : deck;
}

function placeDraw(
  effect: ActiveEffect,
  spec: Extract<ActiveEffect['spec'], { kind: 'draw' }>,
  anchors: Anchors,
): Placed | null {
  const deck = anchors.box('deck');
  if (!deck) {
    return null;
  }
  const mine = spec.playerId === anchors.meId;
  const seat = seatOf(anchors, spec.playerId);
  const count = spec.count;
  const stagger = effect.staggerMs;
  const flightMs = effect.flightMs;
  const flights: Flight[] = [];
  for (let index = 0; index < count; index++) {
    const card = mine ? (spec.cards?.[index] ?? null) : null;
    const landing = mine
      ? ((card ? anchors.handCard(card.id) : null) ?? handFallback(anchors, deck))
      : sizedLike(seat ?? deck, deck, SEAT_CARD_RATIO);
    flights.push({
      from: deck,
      to: landing,
      fromTilt: 0,
      toTilt: 0,
      delayMs: index * stagger,
      durationMs: flightMs,
      card,
      look: card ? 'back-to-face' : 'back',
      settle: false,
    });
  }
  return {
    id: effect.id,
    durationMs: effect.durationMs,
    reduced: effect.reduced,
    kind: 'flights',
    flights,
  };
}

/** Un effet qui vise un joueur : en grand au centre chez lui, sur son siège avec son nom chez les autres. */
function placePersonalLabel(
  base: { readonly id: number; readonly durationMs: number; readonly reduced: boolean },
  anchors: Anchors,
  targetId: string,
  variant: 'caught' | 'challenge',
  text: { readonly mine: string; readonly theirs: (name: string) => string },
): Placed | null {
  if (targetId === anchors.meId) {
    const table = anchors.box('table');
    return (
      table && {
        ...base,
        kind: 'label',
        variant,
        text: text.mine,
        tone: null,
        where: 'center',
        at: table,
      }
    );
  }
  const seat = seatOf(anchors, targetId);
  return (
    seat && {
      ...base,
      kind: 'label',
      variant: 'seat',
      text: text.theirs(anchors.nameOf(targetId)),
      tone: null,
      where: 'seat',
      at: seat,
    }
  );
}

/** Traduit un effet en positions d'écran ; `null` si ce qu'il vise n'est pas à l'écran (alors on ne montre rien). */
export function placeEffect(effect: ActiveEffect, anchors: Anchors): Placed | null {
  const { spec } = effect;
  const base = { id: effect.id, durationMs: effect.durationMs, reduced: effect.reduced };
  const at = (anchor: string): Box | null => anchors.box(anchor);

  switch (spec.kind) {
    case 'play':
      return placePlay(effect, spec.card, spec.playerId, anchors);
    case 'draw':
      return placeDraw(effect, spec, anchors);
    case 'bigText': {
      const tone: Tone = spec.color ?? 'white';
      // Pénalité qui vise un joueur : en grand chez lui seulement ; les autres la voient sur son siège, avec son nom
      if (spec.victimId !== null && spec.victimId !== anchors.meId) {
        const seat = seatOf(anchors, spec.victimId);
        const text = `${spec.text} : ${anchors.nameOf(spec.victimId)} pioche ${spec.amount}`;
        return (
          seat && { ...base, kind: 'label', variant: 'seat', text, tone, where: 'seat', at: seat }
        );
      }
      const table = at('table');
      const caption = spec.victimId === null ? null : `Tu pioches ${spec.amount}`;
      return table && { ...base, kind: 'bigText', text: spec.text, caption, tone, at: table };
    }
    case 'wheel': {
      const table = at('table');
      return table && { ...base, kind: 'wheel', color: spec.color, at: table };
    }
    case 'challenge':
      return placePersonalLabel(base, anchors, spec.penalizedId, 'challenge', {
        mine: `${spec.succeeded ? 'Bluff découvert' : 'Contestation ratée'} : tu pioches ${spec.amount}`,
        theirs: (name) =>
          `${spec.succeeded ? 'Bluff découvert' : 'Contestation ratée'} : ${name} pioche ${spec.amount}`,
      });
    case 'skip': {
      const mine = spec.playerId === anchors.meId;
      const where = mine ? at('table') : seatOf(anchors, spec.playerId);
      const caption = mine
        ? 'Tu passes ton tour'
        : `${anchors.nameOf(spec.playerId)} passe son tour`;
      return (
        where && { ...base, kind: 'skip', caption, where: mine ? 'center' : 'seat', at: where }
      );
    }
    case 'uno': {
      const seat = seatOf(anchors, spec.playerId);
      return (
        seat && {
          ...base,
          kind: 'label',
          variant: 'uno',
          text: 'UNO !',
          tone: null,
          where: 'seat',
          at: seat,
        }
      );
    }
    case 'caught':
      return placePersonalLabel(base, anchors, spec.targetId, 'caught', {
        mine: `Contre-UNO : tu pioches ${spec.amount}`,
        theirs: (name) => `Contre-UNO sur ${name} : ${spec.amount} cartes`,
      });
    case 'turn': {
      const to = seatOf(anchors, spec.to);
      const from = spec.from === null ? to : seatOf(anchors, spec.from);
      return to && from && { ...base, kind: 'orb', from, to };
    }
    case 'spotlight': {
      const seat = seatOf(anchors, spec.playerId);
      return seat && { ...base, kind: 'spotlight', at: seat };
    }
    case 'reverse': {
      const table = at('table');
      return table && { ...base, kind: 'reverse', direction: spec.direction, at: table };
    }
  }
}
