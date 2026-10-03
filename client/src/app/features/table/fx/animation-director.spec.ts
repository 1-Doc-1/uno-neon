import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, vi } from 'vitest';
import type { Card, ClientEvent } from '../../../protocol/generated/protocol';
import { AnimationDirector, MAX_BACKLOG_MS } from './animation-director';
import { planEffects, INITIAL_PLAN_STATE, REDUCED_MS } from './effect-plan';

const red7: Card = { id: 7, color: 'red', rank: '7' };
const redDrawTwo: Card = { id: 8, color: 'red', rank: 'drawTwo' };
const wild: Card = { id: 9, color: null, rank: 'wild' };

const played = (playerId: string, card: Card, chosenColor: 'blue' | null = null): ClientEvent => ({
  kind: 'cardPlayed',
  playerId,
  card,
  chosenColor,
  isJumpIn: false,
});
const turnTo = (playerId: string): ClientEvent => ({ kind: 'turnChanged', playerId });
const drew = (playerId: string, count: number, cards?: Card[]): ClientEvent => ({
  kind: 'cardsDrawn',
  playerId,
  count,
  ...(cards ? { cards } : {}),
});

const live = { resync: false, previousColor: null } as const;

describe('planEffects', () => {
  it('turns events into effects in the order the server sent them', () => {
    const { specs } = planEffects(
      [played('loic', red7), turnTo('zoe'), { kind: 'playerSkipped', playerId: 'zoe' }],
      INITIAL_PLAN_STATE,
    );

    expect(specs.map((spec) => spec.kind)).toEqual(['play', 'turn', 'skip']);
  });

  it('shows the "+2" then marks the draw that follows as a penalty, but not a voluntary draw', () => {
    const { specs, state } = planEffects(
      [played('loic', redDrawTwo), drew('zoe', 2), drew('zoe', 1)],
      INITIAL_PLAN_STATE,
    );

    expect(specs.map((spec) => spec.kind)).toEqual(['play', 'bigText', 'draw', 'draw']);
    expect(specs.filter((spec) => spec.kind === 'draw').map((spec) => spec.penalty)).toEqual([
      true,
      false,
    ]);
    expect(state.penaltyPending).toBe(false);
  });

  it('remembers a penalty announced in an earlier batch (a +4 accepted later)', () => {
    const first = planEffects(
      [{ kind: 'unoCaught', catcherId: 'a', targetId: 'b', penaltyAmount: 2 }],
      INITIAL_PLAN_STATE,
    );
    const second = planEffects([drew('b', 2)], first.state);

    expect(second.specs[0]).toMatchObject({ kind: 'draw', penalty: true });
  });

  it('plays a single colour wheel for a Joker, whether or not the server also sends colorChosen', () => {
    const alone = planEffects([played('loic', wild, 'blue')], INITIAL_PLAN_STATE).specs;
    const doubled = planEffects(
      [played('loic', wild, 'blue'), { kind: 'colorChosen', playerId: 'loic', color: 'blue' }],
      INITIAL_PLAN_STATE,
    ).specs;

    expect(alone.filter((spec) => spec.kind === 'wheel')).toHaveLength(1);
    expect(doubled.filter((spec) => spec.kind === 'wheel')).toHaveLength(1);
  });

  it('does not announce a turn that did not change', () => {
    const first = planEffects([turnTo('zoe')], INITIAL_PLAN_STATE);
    const again = planEffects([turnTo('zoe')], first.state);

    expect(again.specs).toEqual([]);
  });
});

describe('AnimationDirector', () => {
  let director: AnimationDirector;
  const kinds = () => director.active().map((effect) => effect.spec.kind);

  beforeEach(() => {
    vi.useFakeTimers();
    TestBed.configureTestingModule({ providers: [AnimationDirector] });
    director = TestBed.inject(AnimationDirector);
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('starts the first effect at once and chains the next ones in order, small ones overlapping', () => {
    director.enqueue([played('loic', red7), turnTo('zoe')], live);

    expect(kinds()).toEqual(['play']);
    vi.advanceTimersByTime(320);
    expect(kinds()).toEqual(['play', 'turn']);
    vi.advanceTimersByTime(80);
    expect(kinds()).toEqual(['turn']);
    vi.advanceTimersByTime(1000);
    expect(kinds()).toEqual([]);
  });

  it('keeps the queue in order across updates', () => {
    director.enqueue([played('loic', redDrawTwo)], live);
    director.enqueue([drew('zoe', 2)], live);
    const started = new Map<number, string>();
    for (let elapsed = 0; elapsed < 2000; elapsed += 10) {
      for (const effect of director.active()) {
        started.set(effect.id, effect.spec.kind);
      }
      vi.advanceTimersByTime(10);
    }

    expect([...started.values()]).toEqual(['play', 'bigText', 'draw']);
  });

  it('never shows two full-screen effects at once', () => {
    director.enqueue(
      [played('loic', redDrawTwo), { kind: 'directionChanged', direction: 'counterClockwise' }],
      live,
    );
    const fullscreen = new Set(['bigText', 'reverse', 'wheel', 'challenge', 'spotlight']);
    for (let elapsed = 0; elapsed < 2000; elapsed += 10) {
      expect(kinds().filter((kind) => fullscreen.has(kind)).length).toBeLessThanOrEqual(1);
      vi.advanceTimersByTime(10);
    }
  });

  it('flushes everything on a full view: queue, effects, hidden cards', () => {
    director.enqueue([played('loic', red7), turnTo('zoe')], live);
    expect(director.hiddenCardIds().has(red7.id)).toBe(true);

    director.enqueue([played('zoe', redDrawTwo)], { resync: true, previousColor: null });

    expect(kinds()).toEqual([]);
    expect(director.hiddenCardIds().size).toBe(0);
    vi.advanceTimersByTime(5000);
    expect(kinds()).toEqual([]);
  });

  it('drops a queue that is too far behind instead of replaying it late', () => {
    const burst = Array.from({ length: 12 }, (_, index) =>
      played('loic', { ...red7, id: 100 + index }),
    );

    director.enqueue(burst, live);

    expect(kinds()).toEqual([]);
    expect(MAX_BACKLOG_MS).toBeGreaterThan(0);
  });

  it('does not drop a queue just because the effects were slowed down', () => {
    director.speed.set(0.5);

    director.enqueue([played('loic', redDrawTwo), drew('zoe', 2), turnTo('me')], live);

    expect(kinds()).toEqual(['play']);
  });

  it('hides a flying card until it lands, then shows it', () => {
    director.enqueue([played('loic', red7)], live);
    expect(director.hiddenCardIds().has(red7.id)).toBe(true);

    vi.advanceTimersByTime(400);

    expect(director.hiddenCardIds().has(red7.id)).toBe(false);
  });

  it('holds the previous colour while the wheel turns, then lets the rim take the new one', () => {
    director.enqueue([played('loic', wild, 'blue')], { resync: false, previousColor: 'red' });
    expect(director.heldColor()).toBe('red');

    vi.advanceTimersByTime(300);
    expect(director.heldColor()).toBe('red');
    vi.advanceTimersByTime(420);
    expect(director.heldColor()).toBeNull();
  });

  it('holds the end-of-round dialog while the spotlight is on', () => {
    director.enqueue([{ kind: 'roundEnded', winnerId: 'zoe', points: 40 }], live);
    expect(director.dialogsHeld()).toBe(true);

    vi.advanceTimersByTime(600);

    expect(director.dialogsHeld()).toBe(false);
  });

  it('replaces every effect with a short fade when motion is reduced', () => {
    director.reduced.set(true);

    director.enqueue([played('loic', red7), turnTo('zoe')], live);

    expect(director.active()[0].durationMs).toBe(REDUCED_MS);
    expect(director.active()[0].reduced).toBe(true);
    vi.advanceTimersByTime(2 * REDUCED_MS + 1);
    expect(kinds()).toEqual([]);
  });

  it('runs faster at a higher speed', () => {
    director.speed.set(2);

    director.enqueue([played('loic', red7), turnTo('zoe')], live);
    vi.advanceTimersByTime(160);

    expect(kinds()).toContain('turn');
    expect(director.active()[0].durationMs).toBe(200);
  });

  it('flushes when the tab goes to the background', () => {
    director.enqueue([played('loic', red7)], live);
    vi.spyOn(document, 'visibilityState', 'get').mockReturnValue('hidden');

    document.dispatchEvent(new Event('visibilitychange'));

    expect(kinds()).toEqual([]);
    vi.restoreAllMocks();
  });
});
