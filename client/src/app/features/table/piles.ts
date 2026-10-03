import { Component, computed, input, output, signal } from '@angular/core';
import type { Card, Color, Direction } from '../../protocol/generated/protocol';
import { CardBack } from '../../ui/card-back';
import { CardFace } from '../../ui/card';
import { COLOR_NAME, SHAPE_CHAR, SHAPE_NAME } from '../../ui/color-meta';
import { ColorSymbol } from '../../ui/color-symbol';
import { Icon } from '../../ui/icon';

/** Centre de la table : pioche, défausse, couleur active en anneau, sens du jeu et pénalité en attente. */
@Component({
  selector: 'app-piles',
  imports: [CardFace, CardBack, ColorSymbol, Icon],
  template: `
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
                <span class="sr-only">Couleur active : </span>{{ names[color] }} {{ chars[color] }}
                <span class="sr-only">({{ shapes[color] }})</span>
              </span>
            </span>
          }
          <span class="direction" [attr.aria-label]="directionLabel()">
            <app-icon [name]="direction() === 'clockwise' ? 'arrow-cw' : 'arrow-ccw'" />
          </span>
        </p>
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

  /** Un joker posé montre la couleur choisie dans son anneau. */
  protected readonly chosenColor = computed(() =>
    this.discardTop().color === null ? this.currentColor() : null,
  );
  protected readonly directionLabel = computed(() =>
    this.direction() === 'clockwise' ? 'Sens des aiguilles d’une montre' : 'Sens inverse',
  );
}
