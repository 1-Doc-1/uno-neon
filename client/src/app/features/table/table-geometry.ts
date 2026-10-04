import { seatLayout, SeatPlacement, TableShape } from './seat-layout';

export interface Size {
  readonly w: number;
  readonly h: number;
}

/** Un rectangle en pixels, relatif à la zone de jeu (`.stage`) : bord gauche, bord haut, largeur, hauteur. */
export interface Rect {
  readonly left: number;
  readonly top: number;
  readonly width: number;
  readonly height: number;
}

export interface Ellipse {
  readonly cx: number;
  readonly cy: number;
  readonly rx: number;
  readonly ry: number;
}

export interface TableGeometry {
  /** Les boîtes des sièges (éventail, pastille et badges compris), dans l'ordre des adversaires. */
  readonly seats: readonly Rect[];
  /** L'ellipse de la table, dimensionnée d'après la place que les sièges lui laissent. */
  readonly ellipse: Ellipse;
  /** Réduction appliquée aux sièges (1 = taille normale) quand la place manque ; publiée en `--seat-scale`. */
  readonly seatScale: number;
}

/** Rapport hauteur / largeur de l'ellipse (voir `table-center.scss`). */
export const TABLE_SQUASH = 0.65;
/** Distance minimale entre un siège et l'ellipse (le liseré, sa lueur et la flèche du sens débordent un peu). */
export const SEAT_CLEARANCE = 14;

// ---- Les mesures du CSS, reprises ici pour calculer ce que le CSS va dessiner ----
// (opponent-seat.scss, opponent-fan.ts, piles.scss et table-view.scss : un changement là-bas se reporte ici)

const clamp = (min: number, value: number, max: number): number =>
  Math.max(min, Math.min(max, value));

/** `--bw` : la largeur d'un dos de carte d'adversaire ; `--pw` : celle de son avatar. Tailles proportionnelles à la hauteur de l'écran. */
export function seatMetrics(viewportHeight: number): { readonly bw: number; readonly pw: number } {
  return {
    bw: clamp(36, viewportHeight * 0.058, 60),
    pw: clamp(48, viewportHeight * 0.074, 70),
  };
}

const FAN_WIDTH_PER_BACK = 3.6;
const FAN_HEIGHT_PER_BACK = 1.75;
const SEAT_GAP = 4;
/** Pastille : marge, écart avatar/texte, bord ; le nom est tronqué à 120 px (84 px en compact). */
const PILL_EXTRA_WIDTH = 4 + 12 + 16;
const PILL_EXTRA_HEIGHT = 8;
/** Le badge du nombre de cartes déborde sous l'avatar. */
const BADGE_OVERHANG = 10;
/** Le siège dont c'est le tour grossit de 6 %. */
const CURRENT_GROWTH = 1.06;

/** La boîte d'un siège (en pixels) : l'éventail de dos au-dessus de la pastille. */
export function seatSize(
  placement: Pick<SeatPlacement, 'compact' | 'facing'>,
  viewportHeight: number,
  scale = 1,
): Size {
  const { bw, pw } = seatMetrics(viewportHeight);
  const back = bw * scale * (placement.facing ? 1.1 : placement.compact ? 0.78 : 1);
  const portrait = pw * scale * (placement.compact ? 0.8 : 1);
  const nameWidth = placement.compact ? 84 : 120;
  const fanWidth = back * FAN_WIDTH_PER_BACK;
  const pillWidth = portrait + PILL_EXTRA_WIDTH + nameWidth;
  return {
    w: Math.max(fanWidth, pillWidth) * CURRENT_GROWTH,
    h:
      (back * FAN_HEIGHT_PER_BACK + SEAT_GAP + portrait + PILL_EXTRA_HEIGHT) * CURRENT_GROWTH +
      BADGE_OVERHANG,
  };
}

/** Les deux piles (paquet, défausse) côte à côte : ce que l'ellipse doit pouvoir contenir. */
export function pilesSize(viewportHeight: number, narrow = false): Size {
  const card = narrow ? 88 : clamp(96, viewportHeight * 0.155, 168);
  return { w: card * 2.3, h: card * 1.4 + 36 };
}

/** Zone des sièges en mode large : `inset: 2.5vh 0 0` de la zone de jeu. */
const SEATS_TOP_RATIO = 0.025;
/** Zone des sièges en portrait étroit : `inset: 40px 0 auto`, 170 px de haut ; en bande, 56 px au-dessus et 16 en dessous. */
const NARROW_SEATS_TOP = 40;
const NARROW_SEATS_HEIGHT = 170;
const STRIP_PADDING_TOP = 56;
const STRIP_PADDING_BOTTOM = 16;

/** Les tailles de l'ellipse : le plafond de largeur du CSS (`min(62vw, …, 820px)`), plus petit en portrait. */
const MAX_RADIUS_WIDE = 410;
const MAX_RADIUS_NARROW = 170;
const STAGE_MARGIN = 4;
/** En dessous, on réduit les sièges plutôt que de rapetisser encore la table. */
const MIN_RADIUS_RATIO = 0.17;
const SEAT_SCALES = [1, 0.9, 0.8, 0.7, 0.6] as const;

export interface GeometryInput {
  /** La zone de jeu mesurée (ce qui reste de l'écran au-dessus de ma main). */
  readonly stage: Size;
  readonly viewportHeight: number;
  readonly opponents: number;
  readonly shape: TableShape;
  /** Portrait étroit avec plus de quatre adversaires : une bande défilante au lieu d'un arc. */
  readonly strip?: boolean;
}

function seatRects(input: GeometryInput, scale: number): readonly Rect[] {
  const { stage, viewportHeight, shape } = input;
  const placements = seatLayout(input.opponents, shape).map((placement) => ({
    ...placement,
    compact: placement.compact || shape === 'arc',
  }));
  if (input.strip === true) {
    // La bande défilante : tous les sièges sont sur une ligne, au-dessus de la table
    const tallest = Math.max(
      0,
      ...placements.map((placement) => seatSize(placement, viewportHeight, scale).h),
    );
    return [
      {
        left: 0,
        top: 0,
        width: stage.w,
        height: STRIP_PADDING_TOP + tallest + STRIP_PADDING_BOTTOM,
      },
    ];
  }
  const top =
    shape === 'arc' ? NARROW_SEATS_TOP : stage.h > 0 ? viewportHeight * SEATS_TOP_RATIO : 0;
  const height = shape === 'arc' ? NARROW_SEATS_HEIGHT : stage.h - top;
  return placements.map((placement) => {
    const size = seatSize(placement, viewportHeight, scale);
    const cx = (placement.x / 100) * stage.w;
    const cy = top + (placement.y / 100) * height;
    return { left: cx - size.w / 2, top: cy - size.h / 2, width: size.w, height: size.h };
  });
}

/** Vrai si l'ellipse, élargie de `clearance`, touche le rectangle. */
export function ellipseTouches(ellipse: Ellipse, rect: Rect, clearance = 0): boolean {
  const rx = ellipse.rx + clearance;
  const ry = ellipse.ry + clearance;
  const nearestX = clamp(rect.left, ellipse.cx, rect.left + rect.width);
  const nearestY = clamp(rect.top, ellipse.cy, rect.top + rect.height);
  return ((nearestX - ellipse.cx) / rx) ** 2 + ((nearestY - ellipse.cy) / ry) ** 2 < 1;
}

/** Le plus grand rayon horizontal (au rapport de la table) qui tient dans la zone sans toucher un siège, au centre `cy`. */
function largestRadius(stage: Size, seats: readonly Rect[], cy: number, maxRadius: number): number {
  const cx = stage.w / 2;
  const byStage = Math.min(
    stage.w / 2 - STAGE_MARGIN,
    (cy - STAGE_MARGIN) / TABLE_SQUASH,
    (stage.h - STAGE_MARGIN - cy) / TABLE_SQUASH,
    maxRadius,
  );
  if (byStage <= 0) {
    return 0;
  }
  const touches = (rx: number): boolean =>
    seats.some((seat) =>
      ellipseTouches({ cx, cy, rx, ry: rx * TABLE_SQUASH }, seat, SEAT_CLEARANCE),
    );
  if (!touches(byStage)) {
    return byStage;
  }
  let low = 0;
  let high = byStage;
  for (let step = 0; step < 24; step++) {
    const middle = (low + high) / 2;
    if (touches(middle)) {
      high = middle;
    } else {
      low = middle;
    }
  }
  return low;
}

/** L'ellipse la plus grande possible : on essaie chaque hauteur de centre et on garde celle qui laisse le plus de place. */
function bestEllipse(input: GeometryInput, seats: readonly Rect[]): Ellipse {
  const { stage } = input;
  const maxRadius = input.shape === 'arc' ? MAX_RADIUS_NARROW : MAX_RADIUS_WIDE;
  const preferred = stage.h * 0.54;
  let best: Ellipse = { cx: stage.w / 2, cy: preferred, rx: 0, ry: 0 };
  for (let cy = STAGE_MARGIN; cy <= stage.h - STAGE_MARGIN; cy += 2) {
    const rx = largestRadius(stage, seats, cy, maxRadius);
    const better =
      rx > best.rx + 0.5 ||
      (Math.abs(rx - best.rx) <= 0.5 && Math.abs(cy - preferred) < Math.abs(best.cy - preferred));
    if (better) {
      best = { cx: stage.w / 2, cy, rx, ry: rx * TABLE_SQUASH };
    }
  }
  return best;
}

/** Vrai si un rectangle centré sur l'ellipse tient entièrement dedans (les piles doivent pouvoir s'y poser). */
function contains(ellipse: Ellipse, size: Size): boolean {
  return ellipse.rx > 0 && (size.w / 2 / ellipse.rx) ** 2 + (size.h / 2 / ellipse.ry) ** 2 <= 1;
}

function layOut(input: GeometryInput, seatScale: number): TableGeometry {
  const seats = seatRects(input, seatScale);
  return { seats, ellipse: bestEllipse(input, seats), seatScale };
}

/**
 * La mise en page de la table : les sièges sont posés d'après `seatLayout`, puis l'ellipse prend toute la place qu'ils
 * lui laissent, sans en toucher aucun. Si elle devient trop petite pour contenir les piles, les sièges rétrécissent
 * d'un cran et on recommence. Fonction pure : la même que celle qui pilote l'affichage est testée.
 */
export function tableGeometry(input: GeometryInput): TableGeometry {
  const { stage } = input;
  const pileSize = pilesSize(input.viewportHeight, input.shape === 'arc');
  const minRadius = stage.w * MIN_RADIUS_RATIO;
  let result = layOut(input, SEAT_SCALES[0]);
  for (const seatScale of SEAT_SCALES.slice(1)) {
    if (result.ellipse.rx >= minRadius && contains(result.ellipse, pileSize)) {
      break;
    }
    result = layOut(input, seatScale);
  }
  return result;
}
