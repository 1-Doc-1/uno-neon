import { computed, effect, inject, Service, signal } from '@angular/core';
import { DOCUMENT } from '@angular/common';

/**
 * Toutes les durées des animations (SPEC §13), en millisecondes à vitesse normale. Une seule source : le code les lit
 * ici, et `MotionPreferences` les publie en propriétés CSS `--motion-<nom>` (voir `tokens.scss` pour le multiplicateur
 * `--motion-scale`). Rien d'autre dans le client ne code une durée d'animation en dur.
 */
export const MOTION_MS = {
  /** Une carte posée glisse jusqu'à la défausse, s'y pose avec un léger rebond… */
  playFlight: 700,
  /** …puis reste visible un instant avant l'action suivante. */
  playRest: 380,
  /** Le vol d'une carte piochée. L'écart entre deux cartes, lui, vient du serveur (`drawStepMs`, ADR 0026). */
  drawFlight: 520,
  /** Effets spéciaux (+2, +4, Passe, Inversion, Joker) : 1 à 1,4 s. */
  bigText: 1200,
  skip: 1000,
  reverse: 1400,
  wheel: 1200,
  challenge: 1200,
  spotlight: 1400,
  uno: 1100,
  caught: 1100,
  turn: 500,
  /** Pause entre deux effets spéciaux ou tampons. */
  specialGap: 250,
  /** En mouvement réduit : un simple fondu. */
  reduced: 160,
  /** Au-delà de ce retard (à vitesse normale) la file est abandonnée au profit de l'état final. */
  maxBacklog: 9000,
} as const;

/** Au-delà de ce nombre d'étapes en attente, la file accélère pour rattraper son retard au lieu de l'accumuler. */
export const CATCH_UP_THRESHOLD = 3;
export const CATCH_UP_MIN_FACTOR = 0.4;

export type MotionMode = 'normal' | 'fast';

/** Multiplicateur de durée de chaque vitesse (plus petit = plus rapide). */
export const MOTION_SCALE: Record<MotionMode, number> = { normal: 1, fast: 0.6 };

const STORAGE_KEY = 'uno.motion';

/**
 * Le réglage « Vitesse des animations » du joueur : mémorisé dans ce navigateur seulement (jamais envoyé au serveur) et
 * publié en `--motion-scale` sur la page, ce qui ralentit ou accélère aussi les transitions CSS.
 */
@Service()
export class MotionPreferences {
  private readonly root = inject(DOCUMENT).documentElement;
  readonly mode = signal<MotionMode>(read());
  /** Multiplicateur des durées (1 = normale). */
  readonly scale = computed(() => MOTION_SCALE[this.mode()]);

  constructor() {
    for (const [name, ms] of Object.entries(MOTION_MS)) {
      this.root.style.setProperty(`--motion-${kebab(name)}`, `${ms}ms`);
    }
    effect(() => this.root.style.setProperty('--motion-scale', String(this.scale())));
  }

  set(mode: MotionMode): void {
    this.mode.set(mode);
    try {
      localStorage.setItem(STORAGE_KEY, mode);
    } catch {
      // Stockage indisponible : le réglage vaut pour cette page seulement.
    }
  }
}

function read(): MotionMode {
  try {
    return localStorage.getItem(STORAGE_KEY) === 'fast' ? 'fast' : 'normal';
  } catch {
    return 'normal';
  }
}

function kebab(name: string): string {
  return name.replace(/[A-Z]/g, (letter) => `-${letter.toLowerCase()}`);
}
