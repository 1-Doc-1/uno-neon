import { Component, computed, input, output } from '@angular/core';
import type { Card } from '../../protocol/generated/protocol';
import { CardFace } from '../../ui/card';
import { cardLabel } from '../../ui/color-meta';

/** Ma main : les cartes jouables s'illuminent, les autres s'éteignent ; rien ne réagit hors de mon tour. */
@Component({
  selector: 'app-hand',
  imports: [CardFace],
  template: `
    <ul class="hand" [style.--hand-gap]="gap()" aria-label="Ta main">
      @for (card of cards(); track card.id) {
        <li>
          <button
            type="button"
            class="slot"
            [disabled]="!isPlayable(card)"
            [attr.aria-label]="describe(card)"
            (click)="played.emit(card.id)"
          >
            <app-card [card]="card" [state]="stateOf(card)" />
          </button>
        </li>
      }
    </ul>
  `,
  styleUrl: './hand.scss',
})
export class Hand {
  readonly cards = input.required<readonly Card[]>();
  readonly playableIds = input.required<readonly number[]>();
  readonly myTurn = input.required<boolean>();
  readonly played = output<number>();

  /** Au-delà de 8 cartes elles se chevauchent pour tenir sur l'écran. */
  protected readonly gap = computed(() => (this.cards().length > 8 ? '-22px' : '8px'));

  protected isPlayable(card: Card): boolean {
    return this.myTurn() && this.playableIds().includes(card.id);
  }

  protected stateOf(card: Card): 'neutral' | 'playable' | 'unplayable' {
    if (!this.myTurn()) {
      return 'neutral';
    }
    return this.playableIds().includes(card.id) ? 'playable' : 'unplayable';
  }

  protected describe(card: Card): string {
    return this.isPlayable(card) ? `${cardLabel(card)}, jouable` : cardLabel(card);
  }
}
