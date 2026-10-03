import { opponentsInViewOrder, seatSlots } from './seat-layout';

describe('seatSlots', () => {
  it('puts a lone opponent at the top centre', () => {
    const [slot] = seatSlots(1);

    expect(slot.x).toBeCloseTo(50);
    expect(slot.y).toBeCloseTo(50);
  });

  it('spreads opponents left to right, the ends lower than the middle', () => {
    const slots = seatSlots(5);

    expect(slots.map((s) => s.x)).toEqual([...slots.map((s) => s.x)].sort((a, b) => a - b));
    expect(slots[0].y).toBeGreaterThan(slots[2].y);
    expect(slots[0].y).toBeCloseTo(slots[4].y);
    expect(slots[2].x).toBeCloseTo(50);
  });

  it('keeps every seat inside the arc, even with nine opponents', () => {
    for (const slot of seatSlots(9)) {
      expect(slot.x).toBeGreaterThan(0);
      expect(slot.x).toBeLessThan(100);
    }
  });

  it('has nothing to place without opponents', () => {
    expect(seatSlots(0)).toEqual([]);
  });
});

describe('opponentsInViewOrder', () => {
  it('starts with the player who follows me and leaves me out', () => {
    const seats = [0, 1, 2, 3].map((seat) => ({ seat }));

    expect(opponentsInViewOrder(seats, 2).map((s) => s.seat)).toEqual([3, 0, 1]);
    expect(opponentsInViewOrder(seats, 0).map((s) => s.seat)).toEqual([1, 2, 3]);
  });
});
