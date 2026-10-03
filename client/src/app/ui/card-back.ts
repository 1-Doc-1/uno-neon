import { Component } from '@angular/core';
import { APP_NAME } from '../core/app-name';

/** Dos d'une carte : fond uni sombre, wordmark penché au centre, bord néon (SPEC §11.4). */
@Component({
  selector: 'app-card-back',
  template: `
    <svg viewBox="0 0 100 140" aria-hidden="true">
      <rect class="body" x="2" y="2" width="96" height="136" rx="10" />
      <rect class="frame" x="9" y="9" width="82" height="122" rx="6" />
      <g transform="rotate(-18 50 70)">
        <rect class="plate" x="12" y="50" width="76" height="40" rx="10" />
        <text class="word" x="50" y="80" text-anchor="middle">{{ name }}</text>
      </g>
      <rect class="edge" x="2" y="2" width="96" height="136" rx="10" />
    </svg>
  `,
  styles: `
    :host {
      display: block;
      width: var(--card-w, 104px);
      aspect-ratio: 5 / 7;
      color: var(--back-neon);
    }
    svg {
      display: block;
      width: 100%;
      height: 100%;
    }
    .body {
      fill: var(--card-body);
    }
    .frame {
      fill: none;
      stroke: currentColor;
      stroke-opacity: 0.28;
      stroke-width: 1.5;
    }
    .plate {
      fill: var(--game-red);
    }
    .word {
      font-family: var(--font-display);
      font-weight: 700;
      font-size: 32px;
      letter-spacing: 0.04em;
      fill: var(--text-on-neon);
    }
    .edge {
      fill: none;
      stroke: currentColor;
      stroke-width: 3;
      filter: drop-shadow(0 0 2px currentColor);
    }
  `,
})
export class CardBack {
  protected readonly name = APP_NAME;
}
