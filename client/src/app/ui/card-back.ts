import { Component } from '@angular/core';

/** Dos d'une carte : dégradé violet → cyan, circuits fins, monogramme (SPEC §11.4). */
@Component({
  selector: 'app-card-back',
  template: `
    <svg viewBox="0 0 100 140" aria-hidden="true">
      <defs>
        <linearGradient id="back-gradient" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0" style="stop-color: var(--bg-sky-mid)" />
          <stop
            offset="1"
            style="stop-color: color-mix(in oklab, var(--neon-cyan) 55%, var(--bg-sky-top))"
          />
        </linearGradient>
      </defs>
      <rect x="2" y="2" width="96" height="136" rx="10" fill="url(#back-gradient)" />
      <path
        class="circuit"
        d="M14 20 H40 V40 H60 M86 120 H60 V100 H40 M14 70 H30 M70 70 H86 M50 14 V30 M50 126 V110"
      />
      <rect class="edge" x="2" y="2" width="96" height="136" rx="10" />
      <ellipse class="mono-ring" cx="50" cy="70" rx="26" ry="18" transform="rotate(-20 50 70)" />
      <text x="50" y="78" text-anchor="middle" class="mono">U</text>
    </svg>
  `,
  styles: `
    :host {
      --card-w: 104px;
      display: block;
      width: var(--card-w);
      aspect-ratio: 5 / 7;
      color: var(--neon-cyan);
    }
    svg {
      display: block;
      width: 100%;
      height: 100%;
    }
    .edge {
      fill: none;
      stroke: currentColor;
      stroke-width: 2.5;
    }
    .circuit {
      fill: none;
      stroke: rgb(255 255 255 / 0.28);
      stroke-width: 1.5;
    }
    .mono-ring {
      fill: color-mix(in oklab, var(--text-on-neon) 55%, transparent);
      stroke: var(--neon-pink);
      stroke-width: 2.5;
    }
    .mono {
      font-family: var(--font-display);
      font-weight: 900;
      font-size: 28px;
      fill: var(--text-primary);
    }
  `,
})
export class CardBack {}
