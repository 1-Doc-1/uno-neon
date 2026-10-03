import { describeEvent } from './describe-event';

const names: Record<string, string> = { p1: 'Léa', p2: 'Max' };
const nameOf = (id: string): string => names[id] ?? '?';

describe('describeEvent', () => {
  it('tells which card was played, with the shape of its colour and the chosen colour', () => {
    expect(
      describeEvent(
        {
          kind: 'cardPlayed',
          playerId: 'p1',
          card: { id: 1, color: 'red', rank: 'drawTwo' },
          chosenColor: null,
          isJumpIn: false,
        },
        nameOf,
      ),
    ).toBe('Léa pose Plus deux rouge ▲.');

    expect(
      describeEvent(
        {
          kind: 'cardPlayed',
          playerId: 'p2',
          card: { id: 2, color: null, rank: 'wild' },
          chosenColor: 'blue',
          isJumpIn: false,
        },
        nameOf,
      ),
    ).toBe('Max pose Joker → Bleu ◆.');
  });

  it('pluralises drawn cards and ignores turn changes', () => {
    expect(describeEvent({ kind: 'cardsDrawn', playerId: 'p1', count: 1 }, nameOf)).toBe(
      'Léa pioche 1 carte.',
    );
    expect(describeEvent({ kind: 'cardsDrawn', playerId: 'p1', count: 4 }, nameOf)).toBe(
      'Léa pioche 4 cartes.',
    );
    expect(describeEvent({ kind: 'turnChanged', playerId: 'p2' }, nameOf)).toBeNull();
  });

  it('says who pays for a challenge', () => {
    expect(
      describeEvent(
        {
          kind: 'challengeResolved',
          challengerId: 'p1',
          challengedId: 'p2',
          wasBluff: true,
          penalizedPlayerId: 'p2',
          penaltyAmount: 4,
        },
        nameOf,
      ),
    ).toBe('Léa conteste : c’était un bluff, Max pioche 4.');
  });
});
