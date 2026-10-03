export interface SeatSlot {
  /** Position horizontale du centre du siège, en % de la largeur de l'arc. */
  readonly x: number;
  /** Position verticale du centre du siège, en % de la hauteur de l'arc. */
  readonly y: number;
}

const MAX_SPREAD_DEGREES = 62;
const STEP_DEGREES = 26;

/**
 * Répartit `count` adversaires en arc en haut de la table, de gauche à droite : les extrémités descendent un peu,
 * comme des joueurs assis autour d'une table ronde. Un seul adversaire se tient en haut au centre.
 */
export function seatSlots(count: number): readonly SeatSlot[] {
  if (count <= 0) {
    return [];
  }
  const spread = Math.min(MAX_SPREAD_DEGREES, ((count - 1) * STEP_DEGREES) / 2);
  return Array.from({ length: count }, (_, index) => {
    const angle = count === 1 ? 0 : -spread + (index * 2 * spread) / (count - 1);
    const radians = (angle * Math.PI) / 180;
    return {
      x: 50 + 44 * Math.sin(radians),
      y: 50 + 40 * (1 - Math.cos(radians)),
    };
  });
}

/**
 * Les adversaires dans l'ordre où on les voit de gauche à droite : le joueur qui suit le moi (sens des aiguilles
 * d'une montre) est à gauche, celui qui le précède à droite.
 */
export function opponentsInViewOrder<T extends { readonly seat: number }>(
  seats: readonly T[],
  mySeat: number,
): readonly T[] {
  const total = seats.length;
  return seats
    .filter((s) => s.seat !== mySeat)
    .sort((a, b) => ((a.seat - mySeat + total) % total) - ((b.seat - mySeat + total) % total));
}
