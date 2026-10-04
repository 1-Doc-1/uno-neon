import { Component, computed, input } from '@angular/core';
import type { Direction } from '../../../protocol/generated/protocol';
import type { Box } from './effect-geometry';

/**
 * L'inversion du sens : une grande flèche circulaire apparaît au centre de la table, fait un tour dans l'ancien sens,
 * se retourne, puis disparaît. `--turn` vaut 1 si le sens d'avant était horaire, -1 sinon (la flèche part retournée).
 * En mouvement réduit : un simple fondu.
 */
@Component({
  selector: 'app-reverse-arrow',
  template: `
    <svg viewBox="-50 -50 100 100" [class.reduced]="reduced()">
      <path class="ring" d="M0 -34 A34 34 0 1 1 -34 0" />
      <polygon class="tip" points="-34,-14 -45,4 -23,4" />
    </svg>
  `,
  host: {
    '[style.left.px]': 'at().cx',
    '[style.top.px]': 'at().cy',
    '[style.width.px]': 'size()',
    '[style.--turn]': 'direction() === "counterClockwise" ? 1 : -1',
    '[style.--dur]': 'durationMs()',
  },
  styles: `
    :host {
      position: absolute;
      translate: -50% -50%;
    }
    svg {
      display: block;
      overflow: visible;
      fill: none;
      stroke: var(--text);
      stroke-width: 9;
      stroke-linecap: round;
      filter: drop-shadow(0 0 10px var(--text));
      animation: reverse-life calc(var(--dur) * 1ms) cubic-bezier(0.2, 0.9, 0.3, 1) both;
    }
    .tip {
      fill: var(--text);
      stroke-width: 3;
      stroke-linejoin: round;
    }
    .reduced {
      animation-name: fade !important;
    }
    @keyframes reverse-life {
      0% {
        opacity: 0;
        transform: scale(0.4) rotate(0deg) scaleX(var(--turn));
      }
      18% {
        opacity: 1;
        transform: scale(1) rotate(0deg) scaleX(var(--turn));
      }
      62% {
        opacity: 1;
        transform: scale(1) rotate(calc(360deg * var(--turn))) scaleX(var(--turn));
      }
      80% {
        opacity: 1;
        transform: scale(1) rotate(calc(360deg * var(--turn))) scaleX(calc(var(--turn) * -1));
      }
      100% {
        opacity: 0;
        transform: scale(1.1) rotate(calc(360deg * var(--turn))) scaleX(calc(var(--turn) * -1));
      }
    }
    @keyframes fade {
      0%,
      100% {
        opacity: 0;
      }
      30%,
      70% {
        opacity: 1;
      }
    }
  `,
})
export class ReverseArrow {
  readonly direction = input.required<Direction>();
  readonly at = input.required<Box>();
  readonly durationMs = input.required<number>();
  readonly reduced = input(false);

  protected readonly size = computed(() => Math.min(this.at().h * 0.8, 280));
}
