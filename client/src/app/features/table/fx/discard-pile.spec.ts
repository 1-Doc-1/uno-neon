import type { Card, ClientEvent } from '../../../protocol/generated/protocol';
import { discardStack, playedCards, scatterOf, VISIBLE_DISCARDS } from './discard-pile';

const card = (id: number): Card => ({ id, color: 'red', rank: '5' });
const play = (id: number): ClientEvent => ({
  kind: 'cardPlayed',
  playerId: 'a',
  card: card(id),
  chosenColor: null,
  isJumpIn: false,
});

describe('scatterOf', () => {
  it('gives the same untidy placement to the same card every time, and stays within bounds', () => {
    for (let id = 0; id < 200; id++) {
      const first = scatterOf(id);

      expect(scatterOf(id)).toEqual(first);
      expect(Math.abs(first.rotation)).toBeLessThanOrEqual(11);
      expect(Math.abs(first.dx)).toBeLessThanOrEqual(7);
      expect(Math.abs(first.dy)).toBeLessThanOrEqual(7);
    }
  });

  it('does not tilt every card the same way', () => {
    const rotations = new Set(
      Array.from({ length: 20 }, (_, id) => scatterOf(id).rotation.toFixed(2)),
    );

    expect(rotations.size).toBeGreaterThan(10);
  });
});

describe('playedCards', () => {
  it('lists the cards played since the round began, and starts over on a new round or a reshuffle', () => {
    const events: ClientEvent[] = [
      play(1),
      { kind: 'roundStarted', round: 2, dealerId: 'a' },
      play(2),
      play(3),
      { kind: 'deckReshuffled', drawPileCount: 40 },
      play(4),
    ];

    expect(playedCards(events).map((c) => c.id)).toEqual([4]);
    expect(playedCards(events.slice(0, 4)).map((c) => c.id)).toEqual([2, 3]);
  });
});

describe('discardStack', () => {
  it('shows the last few cards with the top one last', () => {
    const played = Array.from({ length: 8 }, (_, index) => card(index + 1));

    const stack = discardStack(played, card(8));

    expect(stack).toHaveLength(VISIBLE_DISCARDS);
    expect(stack.map((item) => item.card.id)).toEqual([4, 5, 6, 7, 8]);
  });

  it('trusts the view for the top card: a history that does not end on it is dropped', () => {
    expect(discardStack([card(1), card(2)], card(9)).map((item) => item.card.id)).toEqual([9]);
    expect(discardStack([], card(9)).map((item) => item.card.id)).toEqual([9]);
  });
});
