import { Component, computed, input } from '@angular/core';

/**
 * Le choc d'une carte de pénalité : un éclair et des ondes qui partent du point d'impact (la défausse), dans la couleur
 * de la carte. Le +4 a plus d'ondes que le +2. Purement décoratif, jamais en mouvement réduit (l'appelant ne l'affiche pas).
 */
@Component({
  selector: 'app-impact-burst',
  template: `
    <span class="flash"></span>
    @for (ring of ringList(); track ring) {
      <span class="ring" [style.--n]="ring"></span>
    }
  `,
  host: { 'aria-hidden': 'true' },
  styles: `
    :host {
      position: absolute;
      width: 0;
      height: 0;
      pointer-events: none;
    }
    .flash {
      position: absolute;
      width: min(120vmin, 1100px);
      aspect-ratio: 1;
      translate: -50% -50%;
      border-radius: 50%;
      background: radial-gradient(
        circle,
        color-mix(in oklab, currentColor 55%, transparent),
        transparent 62%
      );
      animation: flash 380ms ease-out both;
    }

    .ring {
      position: absolute;
      width: min(34vmin, 300px);
      aspect-ratio: 1;
      translate: -50% -50%;
      border: 0.5vmin solid currentColor;
      border-radius: 50%;
      box-shadow:
        0 0 24px currentColor,
        inset 0 0 24px currentColor;
      opacity: 0;
      animation: shockwave 900ms ease-out calc(var(--n) * 130ms) both;
    }

    @keyframes flash {
      0% {
        opacity: 0;
        scale: 0.3;
      }
      25% {
        opacity: 0.9;
      }
      100% {
        opacity: 0;
        scale: 1;
      }
    }

    @keyframes shockwave {
      0% {
        opacity: 0.95;
        scale: 0.2;
      }
      100% {
        opacity: 0;
        scale: 3.4;
      }
    }
  `,
})
export class ImpactBurst {
  readonly rings = input.required<number>();

  protected readonly ringList = computed(() =>
    Array.from({ length: this.rings() }, (_, index) => index),
  );
}
