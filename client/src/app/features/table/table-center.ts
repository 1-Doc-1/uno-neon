import { Component, computed, input } from '@angular/core';
import type { Color, Direction } from '../../protocol/generated/protocol';
import { tintClass } from '../../ui/color-meta';

/**
 * Le tapis : une grande ellipse douce dont le liseré prend discrètement la couleur active, et une lueur qui glisse
 * lentement le long du liseré dans le sens du jeu (coupée si `prefers-reduced-motion`). Les piles sont projetées dedans.
 */
@Component({
  selector: 'app-table-center',
  template: `
    <div class="felt" aria-hidden="true"></div>
    <div class="orbit" aria-hidden="true"><span class="comet"></span></div>
    <ng-content />
  `,
  host: { '[class]': 'tint()', '[class.reverse]': 'direction() === "counterClockwise"' },
  styleUrl: './table-center.scss',
})
export class TableCenter {
  readonly currentColor = input.required<Color | null>();
  readonly direction = input.required<Direction>();

  protected readonly tint = computed(() => tintClass(this.currentColor()));
}
