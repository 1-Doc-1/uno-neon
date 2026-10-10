import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, vi } from 'vitest';
import type { Card, ClientEvent } from '../../../protocol/generated/protocol';
import { AudioService } from '../../../audio/audio.service';
import { MOTION_MS } from '../../../ui/motion';
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

const DRAW_STEP = 1000;
const live = { resync: false, previousColor: null, drawStepMs: DRAW_STEP } as const;

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
      [played('loic', redDrawTwo), drew('zoe', 2), turnTo('zoe'), drew('zoe', 1)],
      INITIAL_PLAN_STATE,
    );

    expect(specs.map((spec) => spec.kind)).toEqual(['play', 'bigText', 'draw', 'turn', 'draw']);
    expect(specs.filter((spec) => spec.kind === 'draw').map((spec) => spec.penalty)).toEqual([
      true,
      false,
    ]);
    expect(state.penaltyPending).toBe(false);
  });

  it('names the player a +2 or a +4 aims at, and gives it the colour of the card played', () => {
    const plusTwo = planEffects(
      [played('loic', { ...redDrawTwo, color: 'blue' }), drew('zoe', 2), turnTo('amy')],
      INITIAL_PLAN_STATE,
    ).specs.find((spec) => spec.kind === 'bigText');
    const plusFour = planEffects(
      [
        played('loic', { id: 11, color: null, rank: 'wildDrawFour' }, 'blue'),
        { kind: 'colorChosen', playerId: 'loic', color: 'blue' },
        turnTo('zoe'),
      ],
      INITIAL_PLAN_STATE,
    ).specs.find((spec) => spec.kind === 'bigText');

    expect(plusTwo).toEqual({
      kind: 'bigText',
      text: '+2',
      amount: 2,
      victimId: 'zoe',
      color: 'blue',
    });
    expect(plusFour).toMatchObject({ text: '+4', amount: 4, victimId: 'zoe', color: null });
  });

  it('remembers a penalty announced in an earlier batch (a +4 accepted later)', () => {
    const first = planEffects(
      [{ kind: 'unoCaught', catcherId: 'a', targetId: 'b', penaltyAmount: 2 }],
      INITIAL_PLAN_STATE,
    );
    const second = planEffects([drew('b', 2)], first.state);

    expect(second.specs[0]).toMatchObject({ kind: 'draw', penalty: true });
  });

  it('shows a Wild Draw Five as a gold effect on its target, and the draw that follows as a penalty', () => {
    const { specs } = planEffects(
      [{ kind: 'plusFiveTargeted', playerId: 'loic', targetId: 'me', total: 10 }, drew('me', 10)],
      INITIAL_PLAN_STATE,
    );

    expect(specs).toMatchObject([
      { kind: 'plusFive', playerId: 'loic', targetId: 'me', total: 10 },
      { kind: 'draw', playerId: 'me', count: 10, penalty: true },
    ]);
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
    localStorage.clear();
    vi.useRealTimers();
  });

  it('starts the first effect at once and lets a played card rest before the next effect', () => {
    director.enqueue([played('loic', red7), turnTo('zoe')], live);

    expect(kinds()).toEqual(['play']);
    vi.advanceTimersByTime(MOTION_MS.playFlight);
    expect(kinds()).toEqual([]);
    vi.advanceTimersByTime(MOTION_MS.playRest);
    expect(kinds()).toEqual(['turn']);
    vi.advanceTimersByTime(MOTION_MS.turn);
    expect(kinds()).toEqual([]);
  });

  describe('a multiple draw, paced by the server', () => {
    const twelve = (playerId: string, cards?: Card[]): ClientEvent[] =>
      Array.from({ length: 12 }, (_, index) =>
        drew(playerId, 1, cards ? [{ ...red7, id: 300 + index }] : undefined),
      );

    it('is one single effect lasting one drawStepMs per card', () => {
      director.enqueue(twelve('me', []), live);

      expect(kinds()).toEqual(['draw']);
      expect(director.active()[0].staggerMs).toBe(DRAW_STEP);
      vi.advanceTimersByTime(11 * DRAW_STEP + MOTION_MS.drawFlight - 1);
      expect(kinds()).toEqual(['draw']);
      vi.advanceTimersByTime(1);
      expect(kinds()).toEqual([]);
    });

    it('brings my cards into my hand, and the counter up, one per drawStepMs', () => {
      director.enqueue(twelve('me', []), live);
      expect(director.hiddenCardIds().size).toBe(12);
      expect(director.unarrived().get('me')).toBe(12);

      vi.advanceTimersByTime(MOTION_MS.drawFlight);
      expect(director.hiddenCardIds().size).toBe(11);
      expect(director.unarrived().get('me')).toBe(11);
      vi.advanceTimersByTime(DRAW_STEP);
      expect(director.hiddenCardIds().size).toBe(10);
      expect(director.unarrived().get('me')).toBe(10);
      vi.advanceTimersByTime(10 * DRAW_STEP);
      expect(director.hiddenCardIds().size).toBe(0);
      expect(director.unarrived().has('me')).toBe(false);
    });

    it("lets everybody watch the cards reach an opponent's seat one by one", () => {
      director.enqueue(twelve('zoe'), live);
      expect(director.unarrived().get('zoe')).toBe(12);

      vi.advanceTimersByTime(MOTION_MS.drawFlight + 3 * DRAW_STEP);
      expect(director.unarrived().get('zoe')).toBe(8);
    });

    it('keeps the pile counter up until each card has left it', () => {
      director.enqueue(twelve('zoe'), live);
      expect(director.unlaunched()).toBe(12);

      vi.advanceTimersByTime(1);
      expect(director.unlaunched()).toBe(11);
      vi.advanceTimersByTime(2 * DRAW_STEP);
      expect(director.unlaunched()).toBe(9);
    });

    it('starts the next effect only when the server is done waiting for the draw', () => {
      director.enqueue([...twelve('me', []), played('me', red7)], live);

      vi.advanceTimersByTime(12 * DRAW_STEP - 1);
      expect(kinds()).not.toContain('play');
      vi.advanceTimersByTime(1);
      expect(kinds()).toContain('play');
    });

    it('is never abandoned for being long, nor sped up to catch up', () => {
      const plays = Array.from({ length: 5 }, (_, index) =>
        played('loic', { ...red7, id: 400 + index }),
      );
      director.enqueue([...twelve('me', []), ...plays.slice(0, 3)], live);
      director.enqueue(plays.slice(3), live);

      expect(kinds()).toEqual(['draw']);
      expect(director.active()[0].staggerMs).toBe(DRAW_STEP);
      expect(director.active()[0].durationMs).toBe(11 * DRAW_STEP + MOTION_MS.drawFlight);
    });

    it('follows a different pace announced by the server', () => {
      director.enqueue(twelve('me', []), { ...live, drawStepMs: 250 });

      expect(director.active()[0].staggerMs).toBe(250);
    });

    it('drops the held counters on a full view', () => {
      director.enqueue(twelve('zoe'), live);

      director.enqueue([], { ...live, resync: true });

      expect(director.unarrived().size).toBe(0);
      expect(director.unlaunched()).toBe(0);
    });

    it('keeps the pace of the server, with a fade as the flight, when motion is reduced', () => {
      director.reduced.set(true);
      director.enqueue(twelve('me', []), live);
      expect(director.unarrived().get('me')).toBe(12);
      expect(director.active()[0].flightMs).toBe(REDUCED_MS);

      vi.advanceTimersByTime(REDUCED_MS + 2 * DRAW_STEP);
      expect(director.unarrived().get('me')).toBe(9);
      vi.advanceTimersByTime(10 * DRAW_STEP);
      expect(director.unarrived().has('me')).toBe(false);
      expect(director.hiddenCardIds().size).toBe(0);
    });
  });

  it('keeps the queue in order across updates', () => {
    director.enqueue([played('loic', redDrawTwo)], live);
    director.enqueue([drew('zoe', 2)], live);
    const started = new Map<number, string>();
    for (let elapsed = 0; elapsed < 6000; elapsed += 10) {
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
    for (let elapsed = 0; elapsed < 6000; elapsed += 10) {
      expect(kinds().filter((kind) => fullscreen.has(kind)).length).toBeLessThanOrEqual(1);
      vi.advanceTimersByTime(10);
    }
  });

  it('flushes everything on a full view: queue, effects, hidden cards', () => {
    director.enqueue([played('loic', red7), turnTo('zoe')], live);
    expect(director.hiddenCardIds().has(red7.id)).toBe(true);

    director.enqueue([played('zoe', redDrawTwo)], { ...live, resync: true });

    expect(kinds()).toEqual([]);
    expect(director.hiddenCardIds().size).toBe(0);
    vi.advanceTimersByTime(8000);
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

    vi.advanceTimersByTime(MOTION_MS.playFlight);

    expect(director.hiddenCardIds().has(red7.id)).toBe(false);
  });

  it('holds the previous colour while the wheel turns, then lets the rim take the new one', () => {
    director.enqueue([played('loic', wild, 'blue')], { ...live, previousColor: 'red' });
    expect(director.heldColor()).toBe('red');

    vi.advanceTimersByTime(MOTION_MS.playFlight + MOTION_MS.playRest + MOTION_MS.wheel * 0.4);
    expect(director.heldColor()).toBe('red');
    vi.advanceTimersByTime(MOTION_MS.wheel * 0.3);
    expect(director.heldColor()).toBeNull();
  });

  it('holds the end-of-round dialog while the spotlight is on', () => {
    director.enqueue([{ kind: 'roundEnded', winnerId: 'zoe', points: 40 }], live);
    expect(director.dialogsHeld()).toBe(true);

    vi.advanceTimersByTime(MOTION_MS.spotlight);

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
    expect(director.active()[0].durationMs).toBe(MOTION_MS.playFlight / 2);
    vi.advanceTimersByTime((MOTION_MS.playFlight + MOTION_MS.playRest) / 2);

    expect(kinds()).toContain('turn');
  });

  it('speeds up to catch up when more than three steps are waiting', () => {
    const plays = Array.from({ length: 5 }, (_, index) =>
      played('loic', { ...red7, id: 200 + index }),
    );

    director.enqueue(plays, live);

    expect(director.active()[0].durationMs).toBeLessThan(MOTION_MS.playFlight);
  });

  it('flushes when the tab goes to the background', () => {
    director.enqueue([played('loic', red7)], live);
    vi.spyOn(document, 'visibilityState', 'get').mockReturnValue('hidden');

    document.dispatchEvent(new Event('visibilitychange'));

    expect(kinds()).toEqual([]);
    vi.restoreAllMocks();
  });
});

describe('AnimationDirector sounds (the director is the only one to ask for them)', () => {
  const rung: string[] = [];
  let director: AnimationDirector;

  beforeEach(() => {
    vi.useFakeTimers();
    rung.length = 0;
    TestBed.configureTestingModule({
      providers: [
        AnimationDirector,
        { provide: AudioService, useValue: { play: (sound: string) => rung.push(sound) } },
      ],
    });
    director = TestBed.inject(AnimationDirector);
  });

  afterEach(() => vi.useRealTimers());

  it('rings one sound per effect, at the moment the effect starts, in step with the animation', () => {
    director.enqueue([played('loic', redDrawTwo), drew('zoe', 2), turnTo('zoe')], {
      ...live,
      meId: 'zoe',
    });

    expect(rung).toEqual(['play']);
    vi.advanceTimersByTime(MOTION_MS.playFlight + MOTION_MS.playRest);
    expect(rung).toEqual(['play', 'plusTwo']);
    // La première carte part un instant après le début de la pioche (une minuterie de 0 ms vaut 1 ms en cours de tic)
    vi.advanceTimersByTime(MOTION_MS.bigText + 1);
    // La pioche sonne carte par carte, au rythme du serveur
    expect(rung).toEqual(['play', 'plusTwo', 'draw']);
    vi.advanceTimersByTime(DRAW_STEP);
    expect(rung).toEqual(['play', 'plusTwo', 'draw', 'draw']);
    vi.advanceTimersByTime(DRAW_STEP);
    expect(rung).toEqual(['play', 'plusTwo', 'draw', 'draw', 'myTurn']);
  });

  it('rings nothing for what a full view throws away', () => {
    director.enqueue([played('loic', red7)], { ...live, resync: true });

    vi.advanceTimersByTime(10_000);

    expect(rung).toEqual([]);
  });

  it('rings the turn only for me, and victory or defeat according to who won', () => {
    director.enqueue([turnTo('zoe'), { kind: 'roundEnded', winnerId: 'me', points: 40 }], {
      ...live,
      meId: 'me',
    });
    vi.advanceTimersByTime(5000);

    expect(rung).toEqual(['win']);
  });
});
