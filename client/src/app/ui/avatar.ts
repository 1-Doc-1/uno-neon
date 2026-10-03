import { Component, computed, input } from '@angular/core';

const TINTS = ['cyan', 'pink', 'violet', 'green', 'yellow', 'blue'] as const;

/** Initiales dans un anneau néon, couleur dérivée de façon déterministe du `playerId` (SPEC §11.5). */
@Component({
  selector: 'app-avatar',
  template: '{{ initials() }}',
  host: { '[class]': '"tint-" + tint()', 'aria-hidden': 'true' },
  styles: `
    :host {
      display: inline-grid;
      place-items: center;
      width: 2.5em;
      height: 2.5em;
      border: 2px solid currentColor;
      border-radius: 50%;
      background: var(--surface-glass-strong);
      box-shadow: var(--glow-sm);
      font-family: var(--font-display);
      font-size: var(--fs-sm);
      font-weight: 700;
      color: var(--neon-cyan);
    }
    :host(.tint-pink) {
      color: var(--neon-pink);
    }
    :host(.tint-violet) {
      color: var(--neon-violet);
    }
    :host(.tint-green) {
      color: var(--neon-green);
    }
    :host(.tint-yellow) {
      color: var(--neon-yellow);
    }
    :host(.tint-blue) {
      color: var(--neon-blue);
    }
  `,
})
export class Avatar {
  readonly playerId = input.required<string>();
  readonly nickname = input.required<string>();

  protected readonly initials = computed(() =>
    [...this.nickname().trim()].slice(0, 2).join('').toUpperCase(),
  );
  protected readonly tint = computed(() => {
    let hash = 0;
    for (const char of this.playerId()) {
      hash = (hash * 31 + char.charCodeAt(0)) % 997;
    }
    return TINTS[hash % TINTS.length];
  });
}
