import {
  afterEveryRender,
  Component,
  computed,
  ElementRef,
  inject,
  input,
  output,
} from '@angular/core';
import type { Card } from '../../protocol/generated/protocol';
import { CardFace } from '../../ui/card';
import { cardLabel } from '../../ui/color-meta';

const MAX_ROTATION_STEP_DEGREES = 3.2;
const TOTAL_FAN_DEGREES = 36;
const MAX_REMEMBERED = 60;

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
      data-anchor="hand"
      aria-label="Ta main"
      [style.grid-template-columns]="columns()"
      [style.--n]="cards().length"
      [style.--rot.deg]="rotationStep()"
    >
      @for (card of cards(); track card.id; let index = $index) {
        <li
          [attr.data-anchor]="'card:' + card.id"
          [class.flying]="hiddenIds().has(card.id)"
          [style.--d]="index - (cards().length - 1) / 2"
        >
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
  /** Les cartes encore en vol vers ma main : présentes mais cachées jusqu'à leur arrivée. */
  readonly hiddenIds = input<ReadonlySet<number>>(new Set());
  readonly played = output<number>();

  private readonly host: HTMLElement = inject(ElementRef).nativeElement;
  private readonly remembered = new Map<number, DOMRect>();

  constructor() {
    // On retient où était chaque carte : une carte jouée a quitté la main avant que l'animation ne démarre
    afterEveryRender({
      read: () => {
        for (const slot of this.host.querySelectorAll<HTMLElement>('li[data-anchor]')) {
          this.remembered.set(
            Number(slot.dataset['anchor']?.slice(5)),
            slot.getBoundingClientRect(),
          );
        }
        while (this.remembered.size > MAX_REMEMBERED) {
          this.remembered.delete(this.remembered.keys().next().value as number);
        }
      },
    });
  }

  /** La dernière position connue d'une carte de ma main, même si elle vient de la quitter. */
  lastRect(cardId: number): DOMRect | null {
    const live = this.host.querySelector('li[data-anchor="card:' + cardId + '"]');
    return live ? live.getBoundingClientRect() : (this.remembered.get(cardId) ?? null);
  }

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
