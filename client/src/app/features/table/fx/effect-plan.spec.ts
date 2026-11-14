import type { ClientEvent } from '../../../protocol/generated/protocol';
import { INITIAL_PLAN_STATE, isFullscreen, planEffects } from './effect-plan';

describe('planEffects, with the 7-0 rule', () => {
  it('shows a swap of two hands, naming both players, after the card that caused it', () => {
    const events: ClientEvent[] = [
      {
        kind: 'cardPlayed',
        playerId: 'a',
        card: { id: 3, rank: '7', color: 'red' },
        chosenColor: null,
      },
      { kind: 'handsSwapped', playerId: 'a', targetId: 'c' },
      { kind: 'turnChanged', playerId: 'b' },
    ];

    const { specs } = planEffects(events, INITIAL_PLAN_STATE);

    expect(specs.map((spec) => spec.kind)).toEqual(['play', 'swap', 'turn']);
    expect(specs[1]).toEqual({ kind: 'swap', playerId: 'a', targetId: 'c' });
  });

  it('shows every hand moving on, in the direction of play', () => {
    const { specs } = planEffects(
      [{ kind: 'handsRotated', direction: 'counterClockwise' }],
      INITIAL_PLAN_STATE,
    );

    expect(specs).toEqual([{ kind: 'rotate', direction: 'counterClockwise' }]);
  });

  it('keeps these effects small: they may overlap the next one, unlike the full-screen ones', () => {
    expect(isFullscreen('swap')).toBe(false);
    expect(isFullscreen('rotate')).toBe(false);
  });
});
