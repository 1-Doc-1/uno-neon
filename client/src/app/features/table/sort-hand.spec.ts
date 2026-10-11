import { describe, expect, it } from 'vitest';
import type { Card } from '../../protocol/generated/protocol';
import { sortHand } from './sort-hand';

const card = (id: number, color: Card['color'], rank: Card['rank']): Card => ({ id, color, rank });

describe('sortHand', () => {
  it('sorts by colour (red, yellow, green, blue), then by value, then actions, jokers last', () => {
    const hand = [
      card(1, null, 'wildDrawFive'),
      card(2, 'blue', '3'),
      card(3, 'red', 'drawTwo'),
      card(4, null, 'wild'),
      card(5, 'green', '9'),
      card(6, 'red', '7'),
      card(7, 'red', 'skip'),
      card(8, 'yellow', '0'),
      card(9, null, 'wildDrawFour'),
      card(10, 'red', 'reverse'),
      card(11, 'red', '2'),
    ];

    expect(sortHand(hand).map((c) => c.id)).toEqual([11, 6, 7, 10, 3, 8, 5, 2, 4, 9, 1]);
  });

  it('is stable for identical cards and does not touch its input', () => {
    const hand = [card(5, 'red', '1'), card(3, 'red', '1')];

    expect(sortHand(hand).map((c) => c.id)).toEqual([3, 5]);
    expect(hand.map((c) => c.id)).toEqual([5, 3]);
  });
});
