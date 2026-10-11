import { SOUNDS } from '../../../audio/sounds';
import { SHOCK_RINGS } from './effects-layer';

const lengthOf = (id: 'plusTwo' | 'plusFour'): number =>
  Math.max(...SOUNDS[id].map((note) => note.at + note.duration));

describe('the impact of a penalty card', () => {
  it('hits harder with a +4 than with a +2: more shock waves', () => {
    expect(SHOCK_RINGS['+4']).toBeGreaterThan(SHOCK_RINGS['+2']);
    expect(SHOCK_RINGS['+2']).toBeGreaterThanOrEqual(2);
  });

  it('gives the +2 and the +4 their own sound: not the same notes, and the +4 is longer and heavier', () => {
    expect(SOUNDS.plusTwo).not.toEqual(SOUNDS.plusFour);
    expect(lengthOf('plusFour')).toBeGreaterThan(lengthOf('plusTwo'));
    const deepest = (id: 'plusTwo' | 'plusFour') =>
      Math.min(...SOUNDS[id].map((n) => n.to ?? n.frequency));
    expect(deepest('plusFour')).toBeLessThan(deepest('plusTwo'));
  });

  it('makes the +4 climb with four hits of rising pitch, where the +2 has two low impacts', () => {
    const rising = SOUNDS.plusFour.filter((note) => note.type === 'sawtooth' && note.at < 0.4);
    const pitches = rising.map((note) => note.frequency);

    expect(pitches).toEqual([...pitches].sort((a, b) => a - b));
    expect(rising.length).toBeGreaterThanOrEqual(4);
    expect(
      SOUNDS.plusTwo.filter((note) => note.type === 'sine' && note.frequency < 200),
    ).toHaveLength(2);
  });
});
