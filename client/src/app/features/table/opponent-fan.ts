import { Component, computed, input } from '@angular/core';
import { CardBack } from '../../ui/card-back';

/** Au-delà de ce nombre de dos de cartes, l'éventail s'arrête et un « +N » prend le relais. */
export const MAX_FAN_BACKS = 12;
const MAX_FAN_DEGREES = 30;
const MAX_STEP_DEGREES = 4;

/**
 * Les cartes d'un adversaire, de dos, en éventail droit (légère courbure seulement). Dessiné d'après le seul nombre de
 * cartes, jamais d'après leur contenu. La taille vient de `--back-w`.
 */
@Component({
  selector: 'app-opponent-fan',
  imports: [CardBack],
  template: `
    @for (back of backs(); track back) {
      <app-card-back class="back" [style.--i]="back" />
    }
    @if (extra() > 0) {
      <span class="more">+{{ extra() }}</span>
    }
  `,
  host: {
    '[style.--step.deg]': 'step()',
    '[style.--count]': 'backs().length',
    'aria-hidden': 'true',
  },
  styles: `
    :host {
      position: relative;
      display: block;
      width: calc(var(--back-w) * 3.4);
      height: calc(var(--back-w) * 1.55);
    }
    .back {
      --card-w: var(--back-w);
      position: absolute;
      left: calc(50% - var(--back-w) / 2);
      bottom: 0;
      transform-origin: 50% 260%;
      transform: rotate(calc((var(--i) - (var(--count) - 1) / 2) * var(--step)));
    }
    .more {
      position: absolute;
      right: 0;
      top: 4px;
      padding: 0 var(--space-2);
      border-radius: var(--radius-pill);
      background: var(--panel-solid);
      font-family: var(--font-display);
      font-weight: 700;
      font-size: var(--fs-sm);
    }
  `,
})
export class OpponentFan {
  readonly count = input.required<number>();
  /** Nombre maximal de dos dessinés (moins sur un petit écran). */
  readonly maxBacks = input(MAX_FAN_BACKS);

  protected readonly backs = computed(() =>
    Array.from({ length: Math.min(this.count(), this.maxBacks()) }, (_, index) => index),
  );
  protected readonly extra = computed(() => Math.max(0, this.count() - this.maxBacks()));
  protected readonly step = computed(() =>
    Math.min(MAX_STEP_DEGREES, MAX_FAN_DEGREES / Math.max(1, this.backs().length - 1)),
  );
}
