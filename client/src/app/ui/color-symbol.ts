import { Component, input } from '@angular/core';
import type { Color } from '../protocol/generated/protocol';

/** Forme associée à une couleur de jeu (daltonisme, SPEC §11.4) : ▲ rouge, ● jaune, ■ vert, ◆ bleu. */
@Component({
  selector: 'app-color-symbol',
  template: `
    <svg viewBox="0 0 24 24" aria-hidden="true" fill="currentColor">
      @switch (color()) {
        @case ('red') {
          <polygon points="12,3 22,21 2,21" />
        }
        @case ('yellow') {
          <circle cx="12" cy="12" r="9.5" />
        }
        @case ('green') {
          <rect x="3.5" y="3.5" width="17" height="17" rx="1.5" />
        }
        @case ('blue') {
          <polygon points="12,1.5 22.5,12 12,22.5 1.5,12" />
        }
      }
    </svg>
  `,
  styles: `
    :host {
      display: inline-block;
      width: 1em;
      height: 1em;
      vertical-align: -0.12em;
    }
    svg {
      display: block;
      width: 100%;
      height: 100%;
    }
  `,
})
export class ColorSymbol {
  readonly color = input.required<Color>();
}
