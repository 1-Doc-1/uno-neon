import { describe, expect, it } from 'vitest';
import { EffectSpec } from './effect-plan';
import { soundOfEffect } from './effect-sound';

const card = { id: 1, color: 'red', rank: '5' } as const;

describe('soundOfEffect', () => {
  const ME = 'me';
  const cases: readonly (readonly [string, EffectSpec, string | null])[] = [
    ['a card played', { kind: 'play', playerId: 'zoe', card }, 'play'],
    ['a +2', { kind: 'bigText', text: '+2', amount: 2, victimId: ME, color: 'red' }, 'plusTwo'],
    ['a +4', { kind: 'bigText', text: '+4', amount: 4, victimId: ME, color: null }, 'plusFour'],
    ['a +5', { kind: 'plusFive', playerId: 'zoe', targetId: ME, total: 5 }, 'plusFive'],
    ['a skip', { kind: 'skip', playerId: ME }, 'skip'],
    ['a reverse', { kind: 'reverse', direction: 'counterClockwise' }, 'reverse'],
    ['a colour choice', { kind: 'wheel', color: 'blue' }, 'color'],
    ['UNO', { kind: 'uno', playerId: 'zoe' }, 'uno'],
    ['a counter-UNO', { kind: 'caught', targetId: 'zoe', amount: 2 }, 'caught'],
    [
      'a challenge',
      { kind: 'challenge', succeeded: true, penalizedId: 'zoe', amount: 4 },
      'caught',
    ],
    ['my turn', { kind: 'turn', from: 'zoe', to: ME }, 'myTurn'],
    ['the turn of somebody else', { kind: 'turn', from: ME, to: 'zoe' }, null],
    ['my victory', { kind: 'spotlight', playerId: ME }, 'win'],
    ['the victory of somebody else', { kind: 'spotlight', playerId: 'zoe' }, 'lose'],
    [
      'a draw (it rings card by card)',
      { kind: 'draw', playerId: ME, count: 3, cards: undefined, penalty: false },
      null,
    ],
  ];

  it.each(cases)('maps %s to its own sound', (_name, spec, expected) => {
    expect(soundOfEffect(spec, ME)).toBe(expected);
  });

  it('has no turn, victory or defeat sound for a spectator without an identity', () => {
    expect(soundOfEffect({ kind: 'turn', from: null, to: 'zoe' }, null)).toBeNull();
    expect(soundOfEffect({ kind: 'spotlight', playerId: 'zoe' }, null)).toBe('lose');
  });
});
