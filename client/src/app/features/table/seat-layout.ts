export interface SeatPlacement {
  /** Centre du siège, en % de la largeur de la zone de jeu (0 = bord gauche). */
  readonly x: number;
  /** Centre du siège, en % de la hauteur de la zone de jeu (0 = bord haut). */
  readonly y: number;
  /** Angle du siège autour de la table, en degrés : 0 = en haut, négatif = à gauche, positif = à droite. */
  readonly angle: number;
  /** Inclinaison de l'éventail de dos, en degrés : il est orienté vers le centre de la table (incliné sur les côtés). */
  readonly rotation: number;
  /** Au-delà de cinq adversaires, les sièges sont compacts. */
  readonly compact: boolean;
}

export type TableShape = 'table' | 'arc';

/** Au-delà de ce nombre d'adversaires, les sièges rétrécissent. */
export const COMPACT_FROM = 6;

interface Ellipse {
  readonly cx: number;
  readonly cy: number;
  readonly rx: number;
  readonly ry: number;
}

// Toute la largeur et toute la hauteur de la zone de jeu (grande ellipse autour du centre de la table) ; en portrait
// étroit, un arc plat qui reste dans le haut de l'écran.
const ELLIPSES: Record<TableShape, Ellipse> = {
  table: { cx: 50, cy: 52, rx: 41, ry: 38 },
  arc: { cx: 50, cy: 60, rx: 34, ry: 24 },
};

const SPREAD_DEGREES = 82;
const SIDE_ROTATION_FACTOR = 0.6;
const MAX_ROTATION = 52;

/** Les angles des sièges, du plus à gauche au plus à droite, selon le nombre d'adversaires. */
function anglesFor(count: number): readonly number[] {
  switch (count) {
    case 1:
      return [0];
    case 2:
      return [-36, 36];
    case 3:
      return [-SPREAD_DEGREES, 0, SPREAD_DEGREES];
    case 4:
      return [-SPREAD_DEGREES, -30, 30, SPREAD_DEGREES];
    default:
      // 5 et plus : à intervalles égaux sur l'arc gauche → haut → droite
      return Array.from(
        { length: count },
        (_, index) => -SPREAD_DEGREES + (index * 2 * SPREAD_DEGREES) / (count - 1),
      );
  }
}

/**
 * Place `count` adversaires autour de la table, de gauche à droite en passant par le haut, comme à une vraie table :
 * un seul est en face ; deux en haut à gauche et en haut à droite ; trois à gauche, en haut et à droite ; quatre en
 * ajoutant les coins ; cinq et plus répartis à intervalles égaux. Une seule fonction pour tous les cas, sur une
 * ellipse, plutôt qu'un cas CSS par nombre de joueurs.
 */
export function seatLayout(count: number, shape: TableShape = 'table'): readonly SeatPlacement[] {
  if (count <= 0) {
    return [];
  }
  const { cx, cy, rx, ry } = ELLIPSES[shape];
  return anglesFor(count).map((angle) => {
    const radians = (angle * Math.PI) / 180;
    return {
      x: cx + rx * Math.sin(radians),
      y: cy - ry * Math.cos(radians),
      angle,
      rotation: Math.max(-MAX_ROTATION, Math.min(MAX_ROTATION, -angle * SIDE_ROTATION_FACTOR)) || 0, // jamais -0
      compact: count >= COMPACT_FROM,
    };
  });
}

/**
 * Les adversaires dans l'ordre où on les voit de gauche à droite : le joueur qui joue après moi (sens des aiguilles
 * d'une montre) est le plus à gauche, celui qui joue juste avant moi le plus à droite.
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
