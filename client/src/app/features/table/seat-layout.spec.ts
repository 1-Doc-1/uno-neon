import { opponentsInViewOrder, seatLayout } from './seat-layout';

describe('seatLayout', () => {
  it('puts a lone opponent facing me, at the top centre', () => {
    const [seat] = seatLayout(1);

    expect(seat.x).toBeCloseTo(50);
    expect(seat.rotation).toBe(0);
    expect(seat.y).toBeLessThan(20);
  });

  it('puts two opponents top left and top right, mirrored', () => {
    const [left, right] = seatLayout(2);

    expect(left.x).toBeLessThan(50);
    expect(right.x).toBeCloseTo(100 - left.x);
    expect(left.y).toBeCloseTo(right.y);
    expect(left.y).toBeLessThan(30);
  });

  it('puts three opponents left, top and right, the sides lower than the top', () => {
    const [left, top, right] = seatLayout(3);

    expect(top.x).toBeCloseTo(50);
    expect(left.x).toBeLessThan(15);
    expect(right.x).toBeGreaterThan(85);
    expect(left.y).toBeGreaterThan(top.y);
    expect(left.y).toBeCloseTo(right.y);
  });

  it('adds the two upper corners for four opponents', () => {
    const seats = seatLayout(4);

    expect(seats.map((s) => s.angle)).toEqual([-82, -30, 30, 82]);
    expect(seats[1].y).toBeLessThan(seats[0].y);
    expect(seats[1].x).toBeGreaterThan(seats[0].x);
  });

  it('spreads five or more at equal angles from left to right, symmetrically', () => {
    for (const count of [5, 6, 9]) {
      const seats = seatLayout(count);
      const gaps = seats.slice(1).map((s, index) => s.angle - seats[index].angle);

      expect(seats).toHaveLength(count);
      expect(seats[0].angle).toBe(-seats[count - 1].angle);
      for (const gap of gaps) {
        expect(gap).toBeCloseTo(gaps[0]);
      }
      expect(seats.map((s) => s.x)).toEqual([...seats.map((s) => s.x)].sort((a, b) => a - b));
    }
  });

  it('tilts the fans toward the centre of the table on the sides, never past 52 degrees', () => {
    const [left, top, right] = seatLayout(3);

    expect(top.rotation).toBe(0);
    expect(left.rotation).toBeGreaterThan(0);
    expect(right.rotation).toBeLessThan(0);
    expect(Math.abs(left.rotation)).toBeLessThanOrEqual(52);
  });

  it('makes the seats compact from six opponents on', () => {
    expect(seatLayout(5).every((s) => !s.compact)).toBe(true);
    expect(seatLayout(6).every((s) => s.compact)).toBe(true);
  });

  it('keeps every seat inside the play area, on the table and in the narrow arc', () => {
    for (const shape of ['table', 'arc'] as const) {
      for (let count = 1; count <= 9; count++) {
        for (const seat of seatLayout(count, shape)) {
          expect(seat.x).toBeGreaterThan(5);
          expect(seat.x).toBeLessThan(95);
          expect(seat.y).toBeGreaterThan(5);
          expect(seat.y).toBeLessThan(95);
        }
      }
    }
  });

  it('keeps the narrow arc shallow, at the top of the screen', () => {
    for (const seat of seatLayout(4, 'arc')) {
      expect(seat.y).toBeLessThan(75);
    }
  });

  it('has nothing to place without opponents', () => {
    expect(seatLayout(0)).toEqual([]);
  });
});

describe('opponentsInViewOrder', () => {
  it('starts with the player who follows me, clockwise, and leaves me out', () => {
    const seats = [0, 1, 2, 3].map((seat) => ({ seat }));

    expect(opponentsInViewOrder(seats, 2).map((s) => s.seat)).toEqual([3, 0, 1]);
    expect(opponentsInViewOrder(seats, 0).map((s) => s.seat)).toEqual([1, 2, 3]);
  });
});
