import { deckSize } from './deck-size';

describe('deckSize', () => {
  it('is 110 for the standard deck: the official 108 cards and two Wild Draw Five', () => {
    expect(
      deckSize({ drawTwoMultiplier: 1, wildDrawFourMultiplier: 1, wildDrawFiveMultiplier: 1 }),
    ).toBe(110);
  });

  it('is 166 when every special card is multiplied by five, as on the server', () => {
    expect(
      deckSize({ drawTwoMultiplier: 5, wildDrawFourMultiplier: 5, wildDrawFiveMultiplier: 5 }),
    ).toBe(166);
  });

  it('counts each special card on its own', () => {
    expect(
      deckSize({ drawTwoMultiplier: 2, wildDrawFourMultiplier: 1, wildDrawFiveMultiplier: 1 }),
    ).toBe(118);
    expect(
      deckSize({ drawTwoMultiplier: 1, wildDrawFourMultiplier: 3, wildDrawFiveMultiplier: 1 }),
    ).toBe(118);
    expect(
      deckSize({ drawTwoMultiplier: 1, wildDrawFourMultiplier: 1, wildDrawFiveMultiplier: 5 }),
    ).toBe(118);
  });
});
