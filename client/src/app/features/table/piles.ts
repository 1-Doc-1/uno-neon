import { Component, computed, input } from '@angular/core';
import type { Card, Color, Direction } from '../../protocol/generated/protocol';
import { CardBack } from '../../ui/card-back';
import { CardFace } from '../../ui/card';
import { COLOR_NAME, SHAPE_CHAR, SHAPE_NAME } from '../../ui/color-meta';
import { ColorSymbol } from '../../ui/color-symbol';

/** Centre de la table : pioche, défausse, couleur courante, sens du jeu et pénalité en attente. */
@Component({
  selector: 'app-piles',
  imports: [CardFace, CardBack, ColorSymbol],
  template: `
    <div class="piles">
      <div class="pile">
        <app-card-back />
        <p class="label"><span class="sr-only">Pioche : </span>{{ drawPileCount() }} cartes</p>
      </div>

      <div class="pile discard" [class]="'discard tint-' + (currentColor() ?? 'wild')">
        <app-card [card]="discardTop()" [chosenColor]="chosenColor()" />
        @if (pendingDraw() > 0) {
          <p class="penalty" role="status">+{{ pendingDraw() }}</p>
        }
      </div>
    </div>

    <p class="status">
      @if (currentColor(); as color) {
        <span [class]="'color tint-' + color">
          <app-color-symbol [color]="color" />
          <span class="color-name"
            ><span class="sr-only">Couleur courante : </span>{{ names[color] }} {{ chars[color] }}
            <span class="sr-only">({{ shapes[color] }})</span></span
          >
        </span>
      }
      <span class="direction" [attr.aria-label]="directionLabel()">{{ directionArrow() }}</span>
    </p>
  `,
  styleUrl: './piles.scss',
})
export class Piles {
  readonly drawPileCount = input.required<number>();
  readonly discardTop = input.required<Card>();
  readonly currentColor = input.required<Color | null>();
  readonly direction = input.required<Direction>();
  readonly pendingDraw = input.required<number>();

  protected readonly names = COLOR_NAME;
  protected readonly chars = SHAPE_CHAR;
  protected readonly shapes = SHAPE_NAME;

  /** Un joker posé montre la couleur choisie dans son anneau. */
  protected readonly chosenColor = computed(() =>
    this.discardTop().color === null ? this.currentColor() : null,
  );
  protected readonly directionArrow = computed(() =>
    this.direction() === 'clockwise' ? '↻' : '↺',
  );
  protected readonly directionLabel = computed(() =>
    this.direction() === 'clockwise' ? 'Sens des aiguilles d’une montre' : 'Sens inverse',
  );
}
