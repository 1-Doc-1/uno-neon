import { describe, expect, it } from 'vitest';
import { keepInside, SCREEN_MARGIN_PX } from './keep-on-screen';

describe('keepInside', () => {
  it('leaves an effect that already fits where it is', () => {
    expect(keepInside(200, 100, 400)).toBe(200);
  });

  it('pushes an effect back from the left and top edges, with a margin', () => {
    expect(keepInside(10, 100, 400)).toBe(50 + SCREEN_MARGIN_PX);
  });

  it('pushes an effect back from the right and bottom edges, with a margin', () => {
    expect(keepInside(395, 100, 400)).toBe(400 - 50 - SCREEN_MARGIN_PX);
  });

  it('centres an effect wider than the screen', () => {
    expect(keepInside(0, 500, 390)).toBe(195);
  });
});
