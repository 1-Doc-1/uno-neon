import { Component, computed, input } from '@angular/core';
import { CardBack } from '../../ui/card-back';

/** Au-delà de ce nombre de dos de cartes, la main s'arrête de grossir : le badge du siège donne le nombre exact. */
export const MAX_FAN_BACKS = 15;
const MAX_SPREAD_DEGREES = 56;
const MAX_STEP_DEGREES = 6;

/**
 * La main d'un adversaire telle qu'on la voit de l'autre côté de la table : des dos de cartes en arc inversé (le pivot
 * est au-dessus, du côté du joueur), avec une légère inclinaison en profondeur (perspective CSS). Dessinée d'après le
 * seul nombre de cartes, jamais d'après leur contenu. La taille vient de `--back-w`. `yaw` tourne la main autour de
 * l'axe vertical, vers le centre de la table pour un siège latéral (jamais de rotation à plat).
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
    '[style.--step.deg]': 'step()',
    '[style.--count]': 'backs().length',
    '[style.--yaw.deg]': 'yaw()',
    'aria-hidden': 'true',
  },
  styles: `
    :host {
      position: relative;
      display: block;
      width: calc(var(--back-w) * 3.6);
      height: calc(var(--back-w) * 1.75);
      perspective: calc(var(--back-w) * 10);
    }
    .hand {
      position: absolute;
      inset: 0;
      transform-style: preserve-3d;
      transform-origin: 50% 0;
      transform: rotateX(var(--tilt, 16deg)) rotateY(var(--yaw));
    }
    .back {
      --card-w: var(--back-w);
      position: absolute;
      left: calc(50% - var(--back-w) / 2);
      top: 0;
      // Pivot au-dessus des cartes : l'arc est inversé par rapport à ma propre main
      transform-origin: 50% -150%;
      transform: rotate(calc((var(--i) - (var(--count) - 1) / 2) * var(--step)));
    }
  `,
})
export class OpponentFan {
  readonly count = input.required<number>();
  /** Nombre maximal de dos dessinés (moins sur un petit écran). */
  readonly maxBacks = input(MAX_FAN_BACKS);
  /** Rotation autour de l'axe vertical, en degrés : 0 en face de moi, vers le centre de la table sur les côtés. */
  readonly yaw = input(0);

  protected readonly backs = computed(() =>
    Array.from({ length: Math.min(this.count(), this.maxBacks()) }, (_, index) => index),
  );
  protected readonly step = computed(() =>
    Math.min(MAX_STEP_DEGREES, MAX_SPREAD_DEGREES / Math.max(1, this.backs().length - 1)),
  );
}
