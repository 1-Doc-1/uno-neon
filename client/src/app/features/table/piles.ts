import { Component, computed, input, output, signal } from '@angular/core';
import type { Card, Color, Direction } from '../../protocol/generated/protocol';
import { CardBack } from '../../ui/card-back';
import { CardFace } from '../../ui/card';
import { COLOR_NAME, SHAPE_NAME, tintClass } from '../../ui/color-meta';
import { ColorSymbol } from '../../ui/color-symbol';
import { discardStack } from './fx/discard-pile';
import { ClickSound } from '../../ui/click-sound';

/**
 * La pioche et la défausse : la pioche est cliquable quand le serveur le permet ; la couleur active entoure la
 * défausse d'une lueur et s'écrit une seule fois, avec sa forme (indispensable après un Joker).
 */
@Component({
  selector: 'app-piles',
  imports: [ClickSound, CardFace, CardBack, ColorSymbol],
  template: `
    <div class="deck-wrap">
      <button
        type="button"
        class="deck"
        data-anchor="deck"
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
      <div class="stack" data-anchor="discard">
        @for (item of stack(); track item.card.id; let top = $last) {
          <div
            class="pile-card"
            [class.in-flight]="hiddenIds().has(item.card.id)"
            [style.--rot.deg]="item.rotation"
            [style.--dx.px]="item.dx"
            [style.--dy.px]="item.dy"
          >
            @if (top) {
              <div [class]="'glow ' + tint()">
                <app-card [card]="item.card" [chosenColor]="chosenColor()" [top]="true" />
              </div>
            } @else {
              <app-card class="under" [card]="item.card" />
            }
          </div>
        }
      </div>
      @if (pendingDraw() > 0) {
        <p class="penalty" role="status">+{{ pendingDraw() }}</p>
      }
      <p class="status">
        @if (currentColor(); as color) {
          <span [class]="'color ' + tint()">
            <app-color-symbol [color]="color" />
            <span class="color-name">
              <span class="sr-only">Couleur active : </span>{{ names[color] }}
              <span class="sr-only">({{ shapes[color] }})</span>
            </span>
          </span>
        }
        <span class="sr-only">{{ directionLabel() }}</span>
      </p>
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
  /** Les cartes posées depuis le début de la manche (d'après les événements) : la défausse montre les dernières en pile désordonnée. */
  readonly played = input<readonly Card[]>([]);
  /** Les cartes encore en vol : présentes dans l'état, cachées jusqu'à leur arrivée. */
  readonly hiddenIds = input<ReadonlySet<number>>(new Set());
  readonly deckClicked = output<void>();

  protected readonly tipShown = signal(false);
  protected readonly stack = computed(() => discardStack(this.played(), this.discardTop()));
  protected readonly tint = computed(() => tintClass(this.currentColor()));
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
        // Une pénalité en attente et la pioche cliquable : on pioche tout ce qui est dû (cumul, ADR 0029)
        return this.pendingDraw() > 0 ? `Piocher ${this.pendingDraw()} cartes` : 'Piocher';
      case null:
        return `Pioche : ${this.drawPileCount()} cartes`;
    }
  });

  protected readonly names = COLOR_NAME;
  protected readonly shapes = SHAPE_NAME;

  /** Un joker posé montre la couleur choisie dans son anneau. */
  protected readonly chosenColor = computed(() =>
    this.discardTop().color === null ? this.currentColor() : null,
  );
  protected readonly directionLabel = computed(() =>
    this.direction() === 'clockwise' ? 'Sens des aiguilles d’une montre' : 'Sens inverse',
  );
}
