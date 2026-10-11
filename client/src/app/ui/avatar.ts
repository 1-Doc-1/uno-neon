import { Component, computed, input } from '@angular/core';

/** Nombre d'avatars proposés (SPEC §11.5, ADR 0037) : le serveur n'accepte que 0 à 11. */
export const AVATAR_COUNT = 12;
export const AVATAR_IDS: readonly number[] = Array.from({ length: AVATAR_COUNT }, (_, id) => id);

const AVATAR_NAMES: readonly string[] = [
  'Chat',
  'Robot',
  'Fantôme',
  'Renard',
  'Hibou',
  'Pieuvre',
  'Fusée',
  'Étoile',
  'Lune',
  'Éclair',
  'Couronne',
  'Champignon',
];
const TINTS = ['red', 'yellow', 'green', 'blue'] as const;

export function avatarName(id: number): string {
  return AVATAR_NAMES[id] ?? AVATAR_NAMES[0];
}

/**
 * Un des 12 avatars du jeu : de petits personnages dessinés en SVG, aux couleurs des tokens du jeu (rien n'est envoyé
 * ni téléversé, seul son numéro voyage, ADR 0037). Décoratif : le pseudo est toujours écrit à côté.
 */
@Component({
  selector: 'app-avatar',
  template: `
    <svg viewBox="0 0 48 48" focusable="false" aria-hidden="true">
      <circle class="back" cx="24" cy="24" r="24" />
      @switch (id()) {
        @case (0) {
          <polygon class="t" points="13,22 14,8 23,16" />
          <polygon class="t" points="35,22 34,8 25,16" />
          <circle class="t" cx="24" cy="27" r="13" />
          <ellipse class="l" cx="19" cy="25" rx="3.4" ry="4" />
          <ellipse class="l" cx="29" cy="25" rx="3.4" ry="4" />
          <circle class="d" cx="19.6" cy="25.6" r="1.8" />
          <circle class="d" cx="28.4" cy="25.6" r="1.8" />
          <polygon class="l" points="22,30 26,30 24,33" />
          <path class="line l" d="M9 29l7 1.5M9 34l7-1.5M39 29l-7 1.5M39 34l-7-1.5" />
        }
        @case (1) {
          <line class="line l" x1="24" y1="7" x2="24" y2="14" />
          <circle class="a" cx="24" cy="7" r="2.6" />
          <rect class="t" x="11" y="14" width="26" height="22" rx="6" />
          <rect class="d" x="15" y="20" width="18" height="8" rx="4" />
          <circle class="a" cx="20" cy="24" r="2.4" />
          <circle class="a" cx="28" cy="24" r="2.4" />
          <rect class="l" x="18" y="31" width="12" height="2.6" rx="1.3" />
          <rect class="t" x="6" y="22" width="4" height="8" rx="2" />
          <rect class="t" x="38" y="22" width="4" height="8" rx="2" />
        }
        @case (2) {
          <path
            class="l"
            d="M11 38V23a13 13 0 0 1 26 0v15l-4.3-3.6L28.5 38l-4.5-3.6L19.5 38l-4.2-3.6z"
          />
          <ellipse class="d" cx="19" cy="23" rx="2.6" ry="3.4" />
          <ellipse class="d" cx="29" cy="23" rx="2.6" ry="3.4" />
          <ellipse class="t" cx="24" cy="29.5" rx="3" ry="2.2" />
        }
        @case (3) {
          <path class="t" d="M8 12l11 7h10l11-7-3 19-13 9-13-9z" />
          <path class="l" d="M15 28l9 12 9-12-9 3z" />
          <circle class="d" cx="18.5" cy="25" r="1.9" />
          <circle class="d" cx="29.5" cy="25" r="1.9" />
          <polygon class="d" points="22,32 26,32 24,35" />
        }
        @case (4) {
          <polygon class="t" points="12,16 13,6 21,12" />
          <polygon class="t" points="36,16 35,6 27,12" />
          <ellipse class="t" cx="24" cy="26" rx="14" ry="15" />
          <circle class="l" cx="18" cy="23" r="6" />
          <circle class="l" cx="30" cy="23" r="6" />
          <circle class="d" cx="18" cy="23" r="2.8" />
          <circle class="d" cx="30" cy="23" r="2.8" />
          <polygon class="a" points="21,29 27,29 24,35" />
        }
        @case (5) {
          <path
            class="line t"
            d="M13 30c-4 4-1 8-5 10M20 32c-2 4 1 6-1 9M28 32c2 4-1 6 1 9M35 30c4 4 1 8 5 10"
          />
          <circle class="t" cx="24" cy="21" r="13" />
          <circle class="l" cx="19" cy="20" r="3.6" />
          <circle class="l" cx="29" cy="20" r="3.6" />
          <circle class="d" cx="19.6" cy="20.6" r="1.7" />
          <circle class="d" cx="28.4" cy="20.6" r="1.7" />
          <path class="line d" d="M20 28q4 3 8 0" />
        }
        @case (6) {
          <polygon class="a" points="20,34 28,34 24,44" />
          <polygon class="t" points="17,28 8,36 18,34" />
          <polygon class="t" points="31,28 40,36 30,34" />
          <path class="l" d="M24 5c8 6 8 17 6 29H18C16 22 16 11 24 5z" />
          <circle class="t" cx="24" cy="19" r="4.4" />
          <circle class="d" cx="24" cy="19" r="2" />
        }
        @case (7) {
          <polygon
            class="a"
            points="24,6 29.2,17.8 42,19 32.4,27.6 35.2,40.2 24,33.6 12.8,40.2 15.6,27.6 6,19 18.8,17.8"
          />
          <circle class="d" cx="20" cy="24" r="1.9" />
          <circle class="d" cx="28" cy="24" r="1.9" />
          <path class="line d" d="M20.5 29q3.5 3 7 0" />
        }
        @case (8) {
          <path class="l" d="M30 7a17 17 0 1 0 11 24A14 14 0 0 1 30 7z" />
          <circle class="d" cx="22" cy="26" r="1.8" />
          <path class="line d" d="M19 32q4 3 8-1" />
          <circle class="a" cx="37" cy="12" r="2" />
          <circle class="t" cx="40" cy="22" r="1.4" />
        }
        @case (9) {
          <polygon class="a" points="27,5 12,27 22,27 19,43 36,20 26,20" />
          <circle class="d" cx="22.5" cy="25" r="1.5" />
          <circle class="d" cx="27.5" cy="25" r="1.5" />
        }
        @case (10) {
          <polygon class="a" points="9,34 11,15 19,24 24,10 29,24 37,15 39,34" />
          <rect class="t" x="9" y="34" width="30" height="5" rx="2" />
          <circle class="t" cx="24" cy="29" r="2.4" />
          <circle class="l" cx="15" cy="29" r="1.6" />
          <circle class="l" cx="33" cy="29" r="1.6" />
        }
        @default {
          <path class="t" d="M7 28a17 15 0 0 1 34 0z" />
          <circle class="l" cx="17" cy="21" r="3" />
          <circle class="l" cx="28" cy="17" r="2.4" />
          <circle class="l" cx="33" cy="24" r="2" />
          <rect class="l" x="18" y="28" width="12" height="11" rx="4" />
          <circle class="d" cx="21.5" cy="33" r="1.4" />
          <circle class="d" cx="26.5" cy="33" r="1.4" />
        }
      }
    </svg>
  `,
  host: { '[class]': '"tint-" + tint()', 'aria-hidden': 'true' },
  styles: `
    :host {
      display: inline-grid;
      width: 2.25em;
      height: 2.25em;
      flex: none;
      border-radius: 50%;
      color: var(--game-blue);
    }
    .tint-red {
      color: var(--game-red);
    }
    .tint-yellow {
      color: var(--game-yellow);
    }
    .tint-green {
      color: var(--game-green);
    }
    svg {
      width: 100%;
      height: 100%;
    }
    .back {
      fill: color-mix(in oklab, var(--field) 62%, currentColor);
    }
    .t {
      fill: currentColor;
    }
    .l {
      fill: var(--text);
    }
    .d {
      fill: var(--field);
    }
    .a {
      fill: var(--uno);
    }
    .line {
      fill: none;
      stroke-width: 2.6;
      stroke-linecap: round;
      stroke-linejoin: round;
    }
    .line.t {
      stroke: currentColor;
    }
    .line.l {
      stroke: var(--text);
    }
    .line.d {
      stroke: var(--field);
    }
  `,
})
export class Avatar {
  /** Le numéro de l'avatar choisi par le joueur (0 à 11). */
  readonly avatarId = input.required<number>();

  protected readonly id = computed(() => {
    const id = Math.trunc(this.avatarId());
    return id >= 0 && id < AVATAR_COUNT ? id : 0;
  });
  protected readonly tint = computed(() => TINTS[this.id() % TINTS.length]);
}
