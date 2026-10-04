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
 * lentement le long du liseré dans le sens du jeu, avec une tête en forme de flèche qui montre ce sens (coupée si
 * `prefers-reduced-motion`). Les piles sont projetées dedans.
 */
@Component({
  selector: 'app-table-center',
  template: `
    <div class="felt" aria-hidden="true"></div>
    <div class="orbit" aria-hidden="true">
      <span #comet class="comet"></span>
    </div>
    <div class="track" aria-hidden="true">
      <svg #head class="head" viewBox="-10 -10 20 20"><polygon points="9,0 -7,-8 -3,0 -7,8" /></svg>
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
  /** Une inversion du sens : la lueur et sa flèche s'éteignent le temps de la grande flèche circulaire, puis repartent à l'envers. */
  readonly burst = input<Burst | null>(null);

  private readonly comet = viewChild.required<ElementRef<HTMLElement>>('comet');
  private readonly head = viewChild.required<ElementRef<SVGElement>>('head');

  protected readonly tint = computed(() => tintClass(this.currentColor()));

  constructor() {
    effect(() => {
      const burst = this.burst();
      if (burst) {
        untracked(() => this.play(burst));
      }
    });
  }

  private play({ durationMs }: Burst): void {
    const fade = [{ opacity: 0 }, { opacity: 0, offset: 0.8 }, { opacity: 1 }];
    for (const glow of [this.comet().nativeElement, this.head().nativeElement]) {
      if (typeof glow.animate === 'function') {
        glow.animate(fade, { duration: durationMs });
      }
    }
  }
}
