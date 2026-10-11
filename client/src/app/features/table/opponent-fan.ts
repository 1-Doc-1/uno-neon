import { Component, computed, input } from '@angular/core';
import { CardBack } from '../../ui/card-back';

/** Au-delà de ce nombre de dos de cartes, la main s'arrête de grossir : le badge du siège donne le nombre exact. */
export const MAX_FAN_BACKS = 15;
/** Un dos de carte est 1,4 fois plus haut que large (ratio 5 / 7). */
export const BACK_HEIGHT_RATIO = 1.4;
/** Décalage de deux cartes voisines, en largeurs de carte, au plus ; il diminue quand la main grossit. */
const MAX_STEP_RATIO = 0.42;
/** Largeur maximale de la main, en largeurs de carte, quand elle est pleine : elle ne dépasse jamais cela. */
const MAX_SPREAD_RATIO = 2.6;

/** Le décalage entre deux cartes d'une main de `count` dos, en largeurs de carte. */
export function fanStepRatio(count: number): number {
  return Math.min(MAX_STEP_RATIO, MAX_SPREAD_RATIO / Math.max(1, count - 1));
}

/** La largeur d'une main de `count` dos, en largeurs de carte (une carte, plus les décalages). */
export function fanWidthRatio(count: number): number {
  return 1 + Math.max(0, count - 1) * fanStepRatio(count);
}

/**
 * La main d'un adversaire telle qu'on la voit sous son profil : des dos de cartes en accordéon, droits, chacun décalé
 * de la précédente (jamais de rotation, même proportions pour tous les sièges). Dessinée d'après le seul nombre de
 * cartes, jamais d'après leur contenu. La taille vient de `--back-w`.
 */
@Component({
  selector: 'app-opponent-fan',
  imports: [CardBack],
  template: `
    <div class="hand">
      @for (back of backs(); track back) {
        <app-card-back class="back" [style.--i]="back" />
      }
    </div>
  `,
  host: {
    '[style.--ratio]': 'step()',
    '[style.--count]': 'backs().length',
    'aria-hidden': 'true',
  },
  styles: `
    :host {
      position: relative;
      display: block;
      width: calc(var(--back-w) * (1 + (var(--count) - 1) * var(--ratio)));
      height: calc(var(--back-w) * 1.4);
    }
    .hand {
      position: absolute;
      inset: 0;
    }
    .back {
      --card-w: var(--back-w);
      position: absolute;
      top: 0;
      left: calc(var(--i) * var(--ratio) * var(--back-w));
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
  protected readonly step = computed(() => fanStepRatio(this.backs().length));
}
