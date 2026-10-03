import { Component, computed, input, output } from '@angular/core';
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
      <button
        type="button"
        class="pile deck"
        [class.usable]="deckAction() !== null"
        [disabled]="deckAction() === null"
        [attr.aria-label]="deckLabel()"
        (click)="deckClicked.emit()"
      >
        <app-card-back />
        <span class="label" aria-hidden="true">{{ deckCaption() }}</span>
      </button>

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
  /** Calculés par le serveur (règle de pioche) : le paquet n'est cliquable que si l'un des deux est vrai. */
  readonly canDraw = input.required<boolean>();
  readonly canKeepDrawnCard = input.required<boolean>();
  readonly deckClicked = output<void>();

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
  protected readonly deckCaption = computed(() =>
    this.deckAction() === null ? `${this.drawPileCount()} cartes` : this.deckLabel(),
  );

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
