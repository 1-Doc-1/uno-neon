import {
  Component,
  computed,
  effect,
  ElementRef,
  input,
  untracked,
  viewChild,
} from '@angular/core';
import type { Color, Direction } from '../../protocol/generated/protocol';
import { tintClass } from '../../ui/color-meta';

export interface Burst {
  readonly id: number;
  readonly durationMs: number;
  readonly reduced: boolean;
}

/**
 * Le tapis : une grande ellipse douce dont le liseré prend discrètement la couleur active, et une lueur qui glisse
 * lentement le long du liseré dans le sens du jeu (coupée si `prefers-reduced-motion`). Les piles sont projetées dedans.
 */
@Component({
  selector: 'app-table-center',
  template: `
    <div class="felt" aria-hidden="true"></div>
    <div class="orbit" aria-hidden="true">
      <span #comet class="comet"></span><span #burst class="burst"></span>
    </div>
    <ng-content />
  `,
  host: {
    'data-anchor': 'table',
    '[class]': 'tint()',
    '[class.reverse]': 'direction() === "counterClockwise"',
  },
  styleUrl: './table-center.scss',
})
export class TableCenter {
  readonly currentColor = input.required<Color | null>();
  readonly direction = input.required<Direction>();
  /** Une inversion du sens : une impulsion lumineuse fait le tour de l'ellipse, puis la lueur repart dans l'autre sens. */
  readonly burst = input<Burst | null>(null);

  private readonly comet = viewChild.required<ElementRef<HTMLElement>>('comet');
  private readonly pulse = viewChild.required<ElementRef<HTMLElement>>('burst');

  protected readonly tint = computed(() => tintClass(this.currentColor()));

  constructor() {
    effect(() => {
      const burst = this.burst();
      if (burst) {
        untracked(() => this.play(burst));
      }
    });
  }

  private play({ durationMs, reduced }: Burst): void {
    const pulse = this.pulse().nativeElement;
    const comet = this.comet().nativeElement;
    if (typeof pulse.animate !== 'function') {
      return;
    }
    // La lueur régulière s'éteint le temps de l'impulsion, puis revient (dans le nouveau sens)
    comet.animate([{ opacity: 0 }, { opacity: 0, offset: 0.8 }, { opacity: 1 }], {
      duration: durationMs,
    });
    if (reduced) {
      pulse.animate([{ opacity: 0 }, { opacity: 1, offset: 0.4 }, { opacity: 0 }], {
        duration: durationMs,
      });
    } else {
      pulse.animate(
        [
          { opacity: 1, transform: 'rotate(0turn)' },
          { opacity: 1, transform: 'rotate(1turn)' },
        ],
        { duration: durationMs, easing: 'cubic-bezier(0.45, 0, 0.2, 1)' },
      );
    }
  }
}
