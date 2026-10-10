import { Component, computed, input } from '@angular/core';
import { APP_NAME } from '../core/app-name';

const TILE_COLORS = ['red', 'yellow', 'green', 'blue'] as const;
const TILE_WIDTH = 46;
const TILE_GAP = 6;
const TILE_HEIGHT = 58;

/**
 * Wordmark original en SVG inline : une tuile arrondie par lettre de `APP_NAME`, légèrement penchées, aux couleurs
 * du jeu, en Fredoka. Ce n'est pas le logo d'un jeu existant (SPEC §1).
 */
@Component({
  selector: 'app-logo',
  template: `
    <svg
      role="img"
      [attr.aria-label]="name"
      [attr.viewBox]="'0 0 ' + width() + ' ' + height"
      [style.height.px]="size()"
    >
      @for (tile of tiles(); track $index) {
        <g class="drop" [style.--i]="$index">
          <g [attr.transform]="tile.transform">
            <rect
              [class]="'tile tint-' + tile.color"
              x="0"
              y="0"
              [attr.width]="tileWidth"
              [attr.height]="tileHeight"
              rx="12"
            />
            <text class="letter" [attr.x]="tileWidth / 2" y="43" text-anchor="middle">
              {{ tile.letter }}
            </text>
          </g>
        </g>
      }
    </svg>
  `,
  host: { '[class.assemble]': 'assemble()' },
  styles: `
    :host {
      display: inline-block;
      line-height: 0;
    }
    .drop {
      transform-box: fill-box;
      transform-origin: center;
    }
    // Cinématique d'introduction : les tuiles tombent l'une après l'autre
    :host(.assemble) .drop {
      animation: tile-in 650ms cubic-bezier(0.2, 0.9, 0.25, 1.2) backwards;
      animation-delay: calc(var(--i) * 170ms);
    }
    @keyframes tile-in {
      from {
        opacity: 0;
        transform: translateY(-90px) rotate(-25deg) scale(1.3);
      }
    }
    @media (prefers-reduced-motion: reduce) {
      :host(.assemble) .drop {
        animation: none;
      }
    }
    svg {
      width: auto;
      overflow: visible;
    }
    .tile {
      fill: currentColor;
    }
    .letter {
      font-family: var(--font-display);
      font-weight: 700;
      font-size: 40px;
      fill: var(--text-on-neon);
    }
  `,
})
export class Logo {
  /** Hauteur de la tuile, en pixels. */
  readonly size = input(44);
  /** Les tuiles tombent l'une après l'autre (cinématique d'introduction). */
  readonly assemble = input(false);

  protected readonly name = APP_NAME;
  protected readonly tileWidth = TILE_WIDTH;
  protected readonly tileHeight = TILE_HEIGHT;
  protected readonly height = TILE_HEIGHT + 12;
  protected readonly width = computed(() => this.tiles().length * (TILE_WIDTH + TILE_GAP) + 4);
  protected readonly tiles = computed(() =>
    [...APP_NAME].map((letter, index) => ({
      letter,
      color: TILE_COLORS[index % TILE_COLORS.length],
      // Chaque tuile penche d'un côté, comme des cartes jetées sur la table
      transform: `translate(${2 + index * (TILE_WIDTH + TILE_GAP)} ${6 + (index % 2) * 2}) rotate(${[-6, 4, -3, 5][index % 4]} ${TILE_WIDTH / 2} ${TILE_HEIGHT / 2})`,
    })),
  );
}
