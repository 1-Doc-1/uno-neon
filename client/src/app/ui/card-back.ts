import { Component } from '@angular/core';
import { APP_NAME } from '../core/app-name';

/** Dos d'une carte : fond uni sombre, wordmark en tube de néon dans un cercle, bord néon (SPEC §11.4). */
@Component({
  selector: 'app-card-back',
  template: `
    <svg viewBox="0 0 100 140" aria-hidden="true">
      <rect class="body" x="2" y="2" width="96" height="136" rx="10" />
      <rect class="frame" x="9" y="9" width="82" height="122" rx="6" />
      <circle class="ring" cx="50" cy="70" r="38" />
      <text class="word" x="50" y="82" text-anchor="middle">{{ name }}</text>
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
    .ring {
      fill: none;
      stroke: currentColor;
      stroke-opacity: 0.45;
      stroke-width: 2;
    }
    .word {
      font-family: var(--font-display);
      font-weight: 700;
      font-size: 28px;
      letter-spacing: 0.04em;
      fill: none;
      stroke: currentColor;
      stroke-width: 1.6;
      stroke-linejoin: round;
      filter: drop-shadow(0 0 2px currentColor);
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
