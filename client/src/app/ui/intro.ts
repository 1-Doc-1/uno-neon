import { Component, DestroyRef, inject, signal } from '@angular/core';
import { CardBack } from './card-back';
import { Logo } from './logo';

const SEEN_KEY = 'uno.introSeen';
const DURATION_MS = 2700;
const REDUCED_DURATION_MS = 1200;
const SKIP_FADE_MS = 250;
const FAN_TINTS = ['red', 'yellow', 'green', 'blue', 'red', 'yellow', 'green'] as const;
const FAN_STEP_DEGREES = 15;

/**
 * La cinématique d'introduction (SPEC §12.1) : à la première ouverture de la session, 2 à 3 secondes, le logo s'assemble,
 * des cartes s'envolent en éventail puis tout s'efface vers l'accueil. Passable d'un clic ou d'une touche, une seule fois
 * par session (sessionStorage), réduite à un fondu avec prefers-reduced-motion. Du CSS et du SVG, rien d'externe.
 * Les navigateurs pilotés par un robot (tests de bout en bout) ne la voient pas.
 */
@Component({
  selector: 'app-intro',
  imports: [CardBack, Logo],
  host: {
    '(document:keydown)': 'skip()',
    '(click)': 'skip()',
  },
  template: `
    @if (visible()) {
      <div class="intro" [class.leaving]="leaving()" [class.reduced]="reduced" aria-hidden="true">
        <div class="stage">
          <div class="fan">
            @for (tint of tints; track $index) {
              <div
                [class]="'flyer tint-' + tint"
                [style.--r.deg]="angle($index)"
                [style.--i]="$index"
              >
                <app-card-back />
              </div>
            }
          </div>
          <app-logo class="mark" [size]="96" [assemble]="true" />
        </div>
        <p class="hint">Touche ou clique pour passer</p>
      </div>
    }
  `,
  styleUrl: './intro.scss',
})
export class Intro {
  protected readonly reduced = prefersReducedMotion();
  protected readonly tints = FAN_TINTS;
  protected readonly visible = signal(shouldPlay());
  protected readonly leaving = signal(false);
  private readonly timers: ReturnType<typeof setTimeout>[] = [];

  constructor() {
    if (this.visible()) {
      remember();
      this.timers.push(
        setTimeout(() => this.skip(), this.reduced ? REDUCED_DURATION_MS : DURATION_MS),
      );
    }
    inject(DestroyRef).onDestroy(() => this.timers.forEach(clearTimeout));
  }

  /** Un clic, une touche ou la fin du temps : un dernier fondu, puis l'accueil. */
  protected skip(): void {
    if (!this.visible() || this.leaving()) {
      return;
    }
    this.leaving.set(true);
    this.timers.push(setTimeout(() => this.visible.set(false), SKIP_FADE_MS));
  }

  protected angle(index: number): number {
    return (index - (FAN_TINTS.length - 1) / 2) * FAN_STEP_DEGREES;
  }
}

function shouldPlay(): boolean {
  // Un robot (Playwright) ne voit pas l'introduction : elle masquerait la page à chaque test
  if (typeof navigator !== 'undefined' && navigator.webdriver) {
    return false;
  }
  try {
    return sessionStorage.getItem(SEEN_KEY) === null;
  } catch {
    return false;
  }
}

function remember(): void {
  try {
    sessionStorage.setItem(SEEN_KEY, '1');
  } catch {
    // Stockage refusé : shouldPlay renvoie alors faux, l'introduction ne reviendra pas
  }
}

function prefersReducedMotion(): boolean {
  return typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches;
}
