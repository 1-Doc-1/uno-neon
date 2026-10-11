import { describe, expect, it } from 'vitest';
import { FULL_SIZE_UP_TO, handScale } from './hand-scale';

describe('handScale', () => {
  it('keeps the normal size up to 16 cards, whatever the width', () => {
    expect(handScale(FULL_SIZE_UP_TO, 300, 72, 36)).toBe(1);
    expect(handScale(3, 100, 72, 36)).toBe(1);
  });

  it('keeps the normal size when the cards fit with a readable step', () => {
    expect(handScale(20, 1600, 72, 36)).toBe(1);
  });

  it('shrinks more when there are more cards or less room', () => {
    const few = handScale(20, 500, 72, 36);
    const many = handScale(40, 500, 72, 36);
    const narrow = handScale(20, 300, 72, 36);

    expect(few).toBeLessThan(1);
    expect(many).toBeLessThan(few);
    expect(narrow).toBeLessThan(few);
  });

  it('makes every card fit: the scaled hand is never wider than the room', () => {
    for (const [count, width] of [
      [20, 700],
      [30, 390],
      [45, 1280],
    ] as const) {
      const scale = handScale(count, width, 72, 36);
      const used = scale * (72 * (1 + 0.4 * (count - 1)) + 2 * 36);
      expect(used).toBeLessThanOrEqual(width + 0.001);
    }
  });

  it('never goes under a floor, so that the cards stay visible', () => {
    expect(handScale(100, 200, 72, 36)).toBeGreaterThanOrEqual(0.3);
  });
});
