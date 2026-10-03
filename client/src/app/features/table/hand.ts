import { Component, computed, input, output } from '@angular/core';
import type { Card } from '../../protocol/generated/protocol';
import { CardFace } from '../../ui/card';
import { cardLabel } from '../../ui/color-meta';

const MAX_ROTATION_STEP_DEGREES = 3.2;
const TOTAL_FAN_DEGREES = 36;

/**
 * Ma main, en éventail : les cartes jouables sont surélevées et brillent, les autres sont légèrement atténuées mais
 * restent lisibles ; rien ne réagit hors de mon tour. Le chevauchement se resserre quand la main grossit.
 */
@Component({
  selector: 'app-hand',
  imports: [CardFace],
  template: `
    <ul
      class="hand"
      aria-label="Ta main"
      [style.grid-template-columns]="columns()"
      [style.--n]="cards().length"
      [style.--rot.deg]="rotationStep()"
    >
      @for (card of cards(); track card.id; let index = $index) {
        <li [style.--d]="index - (cards().length - 1) / 2">
          <button
            type="button"
            class="slot"
            [class.playable]="isPlayable(card)"
            [class.unplayable]="stateOf(card) === 'unplayable'"
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

  /** Chaque carte occupe une colonne d'un pas `--step` (calculé en CSS d'après la largeur), sauf la dernière qui garde sa largeur entière. */
  protected readonly columns = computed(() => {
    const count = this.cards().length;
    return count <= 1 ? 'var(--card-w)' : `repeat(${count - 1}, var(--step)) var(--card-w)`;
  });
  protected readonly rotationStep = computed(() =>
    Math.min(MAX_ROTATION_STEP_DEGREES, TOTAL_FAN_DEGREES / Math.max(1, this.cards().length)),
  );

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
