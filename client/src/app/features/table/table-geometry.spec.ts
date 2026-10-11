import {
  ellipseTouches,
  pilesSize,
  referenceOpponents,
  Rect,
  SEAT_CLEARANCE,
  Size,
  TableGeometry,
  tableGeometry,
} from './table-geometry';
import { MAX_FAN_BACKS } from './opponent-fan';

interface Viewport {
  readonly w: number;
  readonly h: number;
}

const VIEWPORTS: readonly Viewport[] = [
  { w: 1280, h: 720 },
  { w: 1440, h: 900 },
  { w: 1920, h: 1080 },
  { w: 375, h: 812 },
];

/** Max d'adversaires que l'arc d'un portrait étroit accepte : au-delà, une bande défilante (table-view.ts). */
const MAX_ARC_OPPONENTS = 4;

/**
 * La zone de jeu qui reste au-dessus de ma main, mesurée dans le vrai navigateur (la hauteur de ma main : cartes,
 * pastille et phrase « À toi de jouer » ; 261 à 316 px selon l'écran, 285 en portrait étroit).
 */
function stageOf(viewport: Viewport): Size {
  const narrow = viewport.w < 640;
  const handCard = Math.max(100, Math.min(136, viewport.h * 0.14)) * 1.4;
  const mine = narrow ? 290 : handCard + 130;
  return { w: viewport.w, h: viewport.h - mine };
}

function geometryFor(viewport: Viewport, opponents: number): TableGeometry {
  const narrow = viewport.w < 640;
  return tableGeometry({
    stage: stageOf(viewport),
    viewportHeight: viewport.h,
    opponents,
    shape: narrow ? 'arc' : 'table',
    strip: narrow && opponents > MAX_ARC_OPPONENTS,
  });
}

const within = (rect: Rect, stage: Size): boolean =>
  rect.left >= -1 &&
  rect.top >= -1 &&
  rect.left + rect.width <= stage.w + 1 &&
  rect.top + rect.height <= stage.h + 1;

describe('tableGeometry', () => {
  for (const viewport of VIEWPORTS) {
    for (let opponents = 1; opponents <= 9; opponents++) {
      it(`keeps every seat and every opponent hand off the table at ${viewport.w}×${viewport.h} with ${opponents} opponent${opponents > 1 ? 's' : ''}`, () => {
        const { seats, ellipse } = geometryFor(viewport, opponents);

        for (const [index, seat] of seats.entries()) {
          expect(
            ellipseTouches(ellipse, seat, SEAT_CLEARANCE),
            `seat ${index} touches the ellipse`,
          ).toBe(false);
        }
        expect(seats.length).toBe(
          viewport.w < 640 && opponents > MAX_ARC_OPPONENTS ? 1 : opponents,
        );
      });
    }

    it(`lays the table in the play area and leaves it room for both piles at ${viewport.w}×${viewport.h}`, () => {
      const stage = stageOf(viewport);
      const piles = pilesSize(viewport.h, viewport.w < 640);
      // Au-delà de six adversaires l'écran étroit n'a plus la place : seuls les cas courants doivent contenir les piles
      for (const opponents of [1, 2, 3, 4, 5, 6]) {
        const { ellipse } = geometryFor(viewport, opponents);

        expect(ellipse.cx - ellipse.rx).toBeGreaterThanOrEqual(0);
        expect(ellipse.cx + ellipse.rx).toBeLessThanOrEqual(stage.w);
        expect(ellipse.cy - ellipse.ry).toBeGreaterThanOrEqual(0);
        expect(ellipse.cy + ellipse.ry).toBeLessThanOrEqual(stage.h);
        expect(
          (piles.w / 2 / ellipse.rx) ** 2 + (piles.h / 2 / ellipse.ry) ** 2,
        ).toBeLessThanOrEqual(1);
      }
    });
  }

  it('lets the table take the whole room the seats leave it: nothing bigger fits', () => {
    const viewport = VIEWPORTS[0];
    const { seats, ellipse } = geometryFor(viewport, 3);
    const bigger = {
      ...ellipse,
      rx: ellipse.rx + 6,
      ry: (ellipse.rx + 6) * (ellipse.ry / ellipse.rx),
    };
    const stage = stageOf(viewport);

    const blocked =
      seats.some((seat) => ellipseTouches(bigger, seat, SEAT_CLEARANCE)) ||
      bigger.cy - bigger.ry < 0 ||
      bigger.cy + bigger.ry > stage.h ||
      bigger.rx > 410;

    expect(blocked).toBe(true);
  });

  it('keeps the seats inside the play area while there is room', () => {
    for (const viewport of VIEWPORTS.slice(0, 3)) {
      for (const opponents of [1, 2, 3, 4]) {
        const { seats } = geometryFor(viewport, opponents);

        for (const seat of seats) {
          expect(within(seat, stageOf(viewport))).toBe(true);
        }
      }
    }
  });

  it('shrinks the seats, never the guarantee, when the screen is crowded', () => {
    const crowded = geometryFor(VIEWPORTS[0], 9);

    expect(crowded.seatScale).toBeLessThanOrEqual(1);
    expect(crowded.ellipse.rx).toBeGreaterThan(0);
    for (const seat of crowded.seats) {
      expect(ellipseTouches(crowded.ellipse, seat, SEAT_CLEARANCE)).toBe(false);
    }
  });

  it('draws at most as many backs per hand as the opponent fan allows (so seats never outgrow their model)', () => {
    expect(MAX_FAN_BACKS).toBe(15);
  });
});

describe('seat size does not depend on the parity of the opponents (U1)', () => {
  // Cause du défaut : avec 1, 3 ou 5 adversaires un siège se trouve en haut au centre, ce qui réduisait l'ellipse et
  // forçait un cran de réduction des sièges ; avec 2 ou 4, rien ne s'y trouve. La taille était donc plus petite à trois
  // adversaires qu'à quatre. Elle est maintenant choisie d'après un nombre impair (referenceOpponents).
  it('measures an even count as the odd one after it', () => {
    expect([1, 2, 3, 4, 5, 6].map(referenceOpponents)).toEqual([1, 3, 3, 5, 5, 7]);
  });

  for (const viewport of VIEWPORTS) {
    it(`never makes the seats of an odd count smaller than those of the next even count at ${viewport.w}×${viewport.h}`, () => {
      // (5 contre 6 change de catégorie : à partir de six adversaires, tous les sièges sont compacts)
      for (const odd of [1, 3, 7]) {
        const oddScale = geometryFor(viewport, odd).seatScale;
        const evenScale = geometryFor(viewport, odd + 1).seatScale;

        expect(oddScale, `${odd} vs ${odd + 1}`).toBeGreaterThanOrEqual(evenScale - 1e-9);
      }
    });

    it(`gives the same seat size to an even count and the odd count after it at ${viewport.w}×${viewport.h}`, () => {
      for (const even of [2, 4, 6, 8]) {
        expect(geometryFor(viewport, even).seatScale).toBe(
          geometryFor(viewport, even + 1).seatScale,
        );
      }
    });
  }

  it('keeps the seats of three opponents as large as those of four at 1280×720 (the reported case)', () => {
    const viewport = { w: 1280, h: 720 };

    expect(geometryFor(viewport, 3).seatScale).toBeGreaterThanOrEqual(
      geometryFor(viewport, 4).seatScale,
    );
  });
});
