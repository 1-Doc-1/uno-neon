import { Component, computed, input } from '@angular/core';
import { APP_NAME } from '../core/app-name';

const LETTER_WIDTH = 56;
const LETTER_GAP = 10;
const LETTER_HEIGHT = 64;

/**
 * Wordmark original en SVG inline : les lettres de `APP_NAME` en majuscules, tracées comme des tubes de néon creux.
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
      @for (letter of letters(); track $index) {
        <g class="drop" [style.--i]="$index">
          <text class="letter" [attr.x]="letter.x + letterWidth / 2" y="62" text-anchor="middle">
            {{ letter.char }}
          </text>
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
    // Cinématique d'introduction : les lettres s'allument l'une après l'autre
    :host(.assemble) .drop {
      animation: letter-in 650ms cubic-bezier(0.2, 0.9, 0.25, 1.2) backwards;
      animation-delay: calc(var(--i) * 170ms);
    }
    @keyframes letter-in {
      from {
        opacity: 0;
        transform: translateY(-60px) scale(1.25);
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
    .letter {
      font-family: var(--font-display);
      font-weight: 700;
      font-size: 78px;
      letter-spacing: 0.02em;
      fill: none;
      stroke: var(--back-neon);
      stroke-width: 3.5;
      stroke-linejoin: round;
      filter: drop-shadow(0 0 3px var(--back-neon)) drop-shadow(0 0 10px var(--back-neon));
    }
  `,
})
export class Logo {
  /** Hauteur du logo, en pixels. */
  readonly size = input(44);
  /** Les lettres s'allument l'une après l'autre (cinématique d'introduction). */
  readonly assemble = input(false);

  protected readonly name = APP_NAME;
  protected readonly letterWidth = LETTER_WIDTH;
  protected readonly height = LETTER_HEIGHT + 16;
  protected readonly letters = computed(() =>
    [...APP_NAME].map((char, index) => ({ char, x: 4 + index * (LETTER_WIDTH + LETTER_GAP) })),
  );
  protected readonly width = computed(
    () => this.letters().length * (LETTER_WIDTH + LETTER_GAP) + 4,
  );
}
