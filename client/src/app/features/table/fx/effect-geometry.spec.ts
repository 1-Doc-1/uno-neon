import type { Card } from '../../../protocol/generated/protocol';
import type { ActiveEffect } from './animation-director';
import { Anchors, Box, placeEffect } from './effect-geometry';
import type { EffectSpec } from './effect-plan';

const box = (cx: number, cy: number, w = 100, h = 140): Box => ({ cx, cy, w, h });
const card = (id: number): Card => ({ id, color: 'blue', rank: '3' });

const BOXES: Record<string, Box> = {
  discard: box(500, 300),
  deck: box(350, 300),
  table: box(450, 300, 800, 520),
  hand: box(450, 800, 700, 140),
  'seat:loic': box(150, 120, 160, 60),
  'seat:me': box(80, 850, 160, 60),
  'seat:zoe': box(900, 120, 160, 60),
};

const anchors = (overrides: Partial<Anchors> = {}): Anchors => ({
  meId: 'me',
  nameOf: (playerId) => ({ loic: 'Loïc', zoe: 'Zoé', me: 'Moi' })[playerId] ?? '?',
  layer: { cx: 720, cy: 450, w: 1440, h: 900 },
  box: (name) => BOXES[name] ?? null,
  handCard: () => null,
  ...overrides,
});

const effect = (spec: EffectSpec, extra: Partial<ActiveEffect> = {}): ActiveEffect => ({
  id: 1,
  spec,
  durationMs: 400,
  speed: 1,
  staggerMs: 150,
  flightMs: 300,
  reduced: false,
  ...extra,
});

describe('placeEffect', () => {
  it("flies an opponent's card from their seat, back first then turned over, to the discard", () => {
    const placed = placeEffect(
      effect({ kind: 'play', playerId: 'loic', card: card(7) }),
      anchors(),
    );

    expect(placed).toMatchObject({ kind: 'flights' });
    const flight = placed?.kind === 'flights' ? placed.flights[0] : null;
    expect(flight?.look).toBe('back-to-face');
    expect(flight?.from).toMatchObject({ cx: 150, cy: 120 });
    expect(flight?.from.w).toBeLessThan(BOXES['discard'].w);
    expect(flight?.to.w).toBe(BOXES['discard'].w);
  });

  it('flies my own card face up from where it was in my hand', () => {
    const placed = placeEffect(
      effect({ kind: 'play', playerId: 'me', card: card(7) }),
      anchors({ handCard: (id) => (id === 7 ? box(300, 790) : null) }),
    );

    const flight = placed?.kind === 'flights' ? placed.flights[0] : null;
    expect(flight?.look).toBe('face');
    expect(flight?.from).toMatchObject({ cx: 300, cy: 790 });
  });

  it('draws every card, one after another at the pace of the server, turning mine over when they arrive', () => {
    const cards = Array.from({ length: 8 }, (_, index) => card(20 + index));
    const placed = placeEffect(
      effect(
        { kind: 'draw', playerId: 'me', count: 8, cards, penalty: false },
        { durationMs: 7 * 1000 + 300, staggerMs: 1000, flightMs: 300 },
      ),
      anchors({ handCard: (id) => box(100 + id, 800) }),
    );

    const flights = placed?.kind === 'flights' ? placed.flights : [];
    expect(flights).toHaveLength(8);
    expect(flights.map((flight) => flight.delayMs)).toEqual([
      0, 1000, 2000, 3000, 4000, 5000, 6000, 7000,
    ]);
    expect(flights.every((flight) => flight.durationMs === 300)).toBe(true);
    expect(flights.every((flight) => flight.look === 'back-to-face')).toBe(true);
    expect(flights[0].to).toMatchObject({ cx: 120, cy: 800 });
  });

  it('lands a card whose place in my hand is not known yet on a card-sized spot, never on the whole hand', () => {
    const placed = placeEffect(
      effect({ kind: 'draw', playerId: 'me', count: 1, cards: [card(30)], penalty: false }),
      anchors(),
    );

    const flights = placed?.kind === 'flights' ? placed.flights : [];
    expect(flights[0].from).toMatchObject({ cx: 350, cy: 300 });
    expect(flights[0].to).toMatchObject({ cx: 450, cy: 800 });
    expect(flights[0].to.w).toBeLessThan(200);
  });

  it("sends an opponent's drawn cards face down to their seat, without ever showing a card", () => {
    const placed = placeEffect(
      effect({ kind: 'draw', playerId: 'loic', count: 2, cards: undefined, penalty: true }),
      anchors(),
    );

    const flights = placed?.kind === 'flights' ? placed.flights : [];
    expect(flights.map((flight) => flight.look)).toEqual(['back', 'back']);
    expect(flights.every((flight) => flight.card === null)).toBe(true);
    expect(flights[0].to).toMatchObject({ cx: 150, cy: 120 });
  });

  it('places nothing when its target is not on screen', () => {
    expect(placeEffect(effect({ kind: 'skip', playerId: 'ghost' }), anchors())).toBeNull();
  });

  it('draws the reversal as a big circular arrow at the centre of the table, turning towards the old direction', () => {
    const placed = placeEffect(
      effect({ kind: 'reverse', direction: 'counterClockwise' }),
      anchors(),
    );

    expect(placed).toMatchObject({
      kind: 'reverse',
      direction: 'counterClockwise',
      at: { cx: 450, cy: 300 },
    });
  });

  describe('an effect aimed at ONE player', () => {
    const plusTwo: EffectSpec = {
      kind: 'bigText',
      text: '+2',
      amount: 2,
      victimId: 'me',
      color: 'blue',
    };

    it('is shown big at the centre, in the second person, to the player it targets', () => {
      const placed = placeEffect(effect(plusTwo), anchors());

      expect(placed).toMatchObject({
        kind: 'bigText',
        text: '+2',
        caption: 'Tu pioches 2',
        tone: 'blue',
        at: { cx: 450, cy: 300 },
      });
    });

    it("is shown on the target's seat, with their name, to everybody else", () => {
      const placed = placeEffect(effect({ ...plusTwo, victimId: 'zoe' }), anchors());

      expect(placed).toMatchObject({
        kind: 'label',
        variant: 'seat',
        where: 'seat',
        text: '+2 : Zoé pioche 2',
        tone: 'blue',
        at: { cx: 900, cy: 120 },
      });
    });

    it('is white for a black card, and big for everybody when nobody can be named', () => {
      const placed = placeEffect(
        effect({ ...plusTwo, text: '+4', amount: 4, victimId: null, color: null }),
        anchors(),
      );

      expect(placed).toMatchObject({ kind: 'bigText', tone: 'white', caption: null });
    });

    it('says "Tu passes ton tour" at the centre to the player who is skipped, and names them elsewhere', () => {
      expect(placeEffect(effect({ kind: 'skip', playerId: 'me' }), anchors())).toMatchObject({
        kind: 'skip',
        where: 'center',
        caption: 'Tu passes ton tour',
        at: { cx: 450, cy: 300 },
      });
      expect(placeEffect(effect({ kind: 'skip', playerId: 'loic' }), anchors())).toMatchObject({
        kind: 'skip',
        where: 'seat',
        caption: 'Loïc passe son tour',
        at: { cx: 150, cy: 120 },
      });
    });

    it('stamps "Contre-UNO" big for the player caught, and on their seat for the others', () => {
      expect(
        placeEffect(effect({ kind: 'caught', targetId: 'me', amount: 2 }), anchors()),
      ).toMatchObject({
        kind: 'label',
        variant: 'caught',
        where: 'center',
        text: 'Contre-UNO : tu pioches 2',
      });
      expect(
        placeEffect(effect({ kind: 'caught', targetId: 'zoe', amount: 2 }), anchors()),
      ).toMatchObject({
        kind: 'label',
        variant: 'seat',
        where: 'seat',
        text: 'Contre-UNO sur Zoé : 2 cartes',
      });
    });

    it('gives the verdict of a challenge to the penalised player, at the centre, and names them elsewhere', () => {
      const verdict = (penalizedId: string): EffectSpec => ({
        kind: 'challenge',
        succeeded: false,
        penalizedId,
        amount: 6,
      });

      expect(placeEffect(effect(verdict('me')), anchors())).toMatchObject({
        kind: 'label',
        variant: 'challenge',
        where: 'center',
        text: 'Contestation ratée : tu pioches 6',
      });
      expect(placeEffect(effect(verdict('zoe')), anchors())).toMatchObject({
        kind: 'label',
        variant: 'seat',
        text: 'Contestation ratée : Zoé pioche 6',
      });
    });
  });

  it('plays the effects that concern the whole table at the centre, for everybody', () => {
    const wheel = placeEffect(effect({ kind: 'wheel', color: 'red' }), anchors());
    const wheelElsewhere = placeEffect(
      effect({ kind: 'wheel', color: 'red' }),
      anchors({ meId: 'loic' }),
    );

    expect(wheel).toMatchObject({ kind: 'wheel', at: { cx: 450, cy: 300 } });
    expect(wheelElsewhere).toMatchObject({ kind: 'wheel', at: { cx: 450, cy: 300 } });
  });
});
