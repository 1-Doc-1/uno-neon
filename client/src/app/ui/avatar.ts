import { Component, computed, input } from '@angular/core';

/** Pastille avec l'initiale du joueur, sobre : un disque uni dont la teinte dérive du `playerId` (SPEC §11.5). */
@Component({
  selector: 'app-avatar',
  template: '{{ initial() }}',
  host: { '[class]': '"tint-" + tint()', 'aria-hidden': 'true' },
  styles: `
    :host {
      display: inline-grid;
      place-items: center;
      width: 2.25em;
      height: 2.25em;
      flex: none;
      border-radius: 50%;
      background: color-mix(in oklab, var(--field) 55%, currentColor);
      font-family: var(--font-display);
      font-weight: 700;
      font-size: var(--fs-base);
      color: var(--game-blue);
    }
    :host {
      // Le disque prend la teinte (currentColor) ; l'initiale, elle, reste de la couleur du texte
      -webkit-text-fill-color: var(--text);
    }
  `,
})
export class Avatar {
  readonly playerId = input.required<string>();
  readonly nickname = input.required<string>();

  protected readonly initial = computed(() => [...this.nickname().trim()][0]?.toUpperCase() ?? '?');
  protected readonly tint = computed(() => {
    const tints = ['red', 'yellow', 'green', 'blue'] as const;
    let hash = 0;
    for (const char of this.playerId()) {
      hash = (hash * 31 + char.charCodeAt(0)) % 997;
    }
    return tints[hash % tints.length];
  });
}
