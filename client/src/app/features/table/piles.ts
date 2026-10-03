import { Component, computed, input, output, signal } from '@angular/core';
import type { Card, Color, Direction } from '../../protocol/generated/protocol';
import { CardBack } from '../../ui/card-back';
import { CardFace } from '../../ui/card';
import { COLOR_NAME, SHAPE_CHAR, SHAPE_NAME } from '../../ui/color-meta';
import { ColorSymbol } from '../../ui/color-symbol';

const RING = { cx: 200, cy: 130, rx: 188, ry: 118 };
const ARROW_COUNT = 10;

/**
 * Centre de la table : une zone elliptique discrète, la pioche et la défausse au milieu, la couleur active en anneau
 * autour de la défausse (indispensable après un Joker) et, tout autour, un anneau de flèches qui donne le sens du jeu.
 */
@Component({
  selector: 'app-piles',
  imports: [CardFace, CardBack, ColorSymbol],
  template: `
    <div class="zone">
      <div class="felt" aria-hidden="true"></div>
      <svg class="flow" viewBox="0 0 400 260" aria-hidden="true">
        <ellipse
          class="track"
          [attr.cx]="ring.cx"
          [attr.cy]="ring.cy"
          [attr.rx]="ring.rx"
          [attr.ry]="ring.ry"
        />
        @for (arrow of arrows(); track $index) {
          <path class="arrow" d="M-7 -6 L5 0 L-7 6" [attr.transform]="arrow" />
        }
      </svg>

      <div class="piles">
        <div class="deck-wrap">
          <button
            type="button"
            class="deck"
            [class.usable]="deckAction() !== null"
            [disabled]="deckAction() === null"
            [attr.aria-label]="deckLabel()"
            (click)="deckClicked.emit()"
            (mouseenter)="tipShown.set(true)"
            (mouseleave)="tipShown.set(false)"
            (focus)="tipShown.set(true)"
            (blur)="tipShown.set(false)"
          >
            <app-card-back />
          </button>
          @if (deckAction() !== null && tipShown()) {
            <span class="tip" role="tooltip">{{ deckLabel() }}</span>
          }
          <p class="count"><span class="sr-only">Pioche : </span>{{ drawPileCount() }} cartes</p>
        </div>

        <div class="discard-wrap">
          <div
            [class]="'ring tint-' + (currentColor() ?? 'wild')"
            [class.neutral]="currentColor() === null"
          >
            <app-card [card]="discardTop()" [chosenColor]="chosenColor()" [top]="true" />
          </div>
          @if (pendingDraw() > 0) {
            <p class="penalty" role="status">+{{ pendingDraw() }}</p>
          }
          <p class="status">
            @if (currentColor(); as color) {
              <span [class]="'color tint-' + color">
                <app-color-symbol [color]="color" />
                <span class="color-name">
                  <span class="sr-only">Couleur active : </span>{{ names[color] }}
                  {{ chars[color] }}
                  <span class="sr-only">({{ shapes[color] }})</span>
                </span>
              </span>
            }
            <span class="sr-only">{{ directionLabel() }}</span>
          </p>
        </div>
      </div>
    </div>
  `,
  styleUrl: './piles.scss',
})
export class Piles {
  readonly drawPileCount = input.required<number>();
  readonly discardTop = input.required<Card>();
  readonly currentColor = input.required<Color | null>();
  readonly direction = input.required<Direction>();
  readonly pendingDraw = input.required<number>();
  /** Calculés par le serveur (règle de pioche) : le paquet n'est cliquable que si l'un des deux est vrai. */
  readonly canDraw = input.required<boolean>();
  readonly canKeepDrawnCard = input.required<boolean>();
  readonly deckClicked = output<void>();

  protected readonly ring = RING;
  protected readonly tipShown = signal(false);
  protected readonly deckAction = computed<'draw' | 'keep' | null>(() => {
    if (this.canKeepDrawnCard()) {
      return 'keep';
    }
    return this.canDraw() ? 'draw' : null;
  });
  protected readonly deckLabel = computed(() => {
    switch (this.deckAction()) {
      case 'keep':
        return 'Garder la carte';
      case 'draw':
        return 'Piocher';
      case null:
        return `Pioche : ${this.drawPileCount()} cartes`;
    }
  });

  protected readonly names = COLOR_NAME;
  protected readonly chars = SHAPE_CHAR;
  protected readonly shapes = SHAPE_NAME;

  /** Des pointes de flèche régulièrement réparties sur l'ellipse, tournées dans le sens du jeu. */
  protected readonly arrows = computed(() =>
    Array.from({ length: ARROW_COUNT }, (_, index) => {
      const theta = (index * 2 * Math.PI) / ARROW_COUNT;
      const x = RING.cx + RING.rx * Math.cos(theta);
      const y = RING.cy + RING.ry * Math.sin(theta);
      // Tangente de l'ellipse dans le sens horaire (l'axe y de l'écran pointe vers le bas) ; inversée si le jeu tourne à l'envers
      const tangent =
        (Math.atan2(RING.ry * Math.cos(theta), -RING.rx * Math.sin(theta)) * 180) / Math.PI;
      const heading = this.direction() === 'clockwise' ? tangent : tangent + 180;
      return `translate(${x.toFixed(1)} ${y.toFixed(1)}) rotate(${heading.toFixed(1)})`;
    }),
  );

  /** Un joker posé montre la couleur choisie dans son anneau. */
  protected readonly chosenColor = computed(() =>
    this.discardTop().color === null ? this.currentColor() : null,
  );
  protected readonly directionLabel = computed(() =>
    this.direction() === 'clockwise' ? 'Sens des aiguilles d’une montre' : 'Sens inverse',
  );
}
