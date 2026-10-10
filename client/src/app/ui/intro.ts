import {
  Component,
  DestroyRef,
  effect,
  ElementRef,
  inject,
  signal,
  viewChild,
} from '@angular/core';
import { AudioService } from '../audio/audio.service';
import { CardBack } from './card-back';
import { ClickSound } from './click-sound';
import { Logo } from './logo';

const SEEN_KEY = 'uno.introSeen';
const DURATION_MS = 2700;
const REDUCED_DURATION_MS = 1200;
const SKIP_FADE_MS = 250;
const FAN_TINTS = ['red', 'yellow', 'green', 'blue', 'red', 'yellow', 'green'] as const;
const FAN_STEP_DEGREES = 15;

/**
 * L'ouverture (SPEC §12.1) : à la première ouverture de la session, d'abord un écran « Cliquer pour jouer » (le clic est
 * le geste que les navigateurs exigent avant tout son : il débloque l'audio), puis, 2 à 3 secondes, le logo s'assemble,
 * des cartes s'envolent en éventail, avec un son d'ouverture calé dessus, puis tout s'efface vers l'accueil. Passable d'un clic ou d'une touche, une seule fois
 * par session (sessionStorage), réduite à un fondu avec prefers-reduced-motion. Du CSS et du SVG, rien d'externe.
 * Les navigateurs pilotés par un robot (tests de bout en bout) ne la voient pas.
 */
@Component({
  selector: 'app-intro',
  imports: [CardBack, ClickSound, Logo],
  host: {
    '(document:keydown)': 'skip()',
    '(click)': 'skip()',
  },
  template: `
    @if (gated()) {
      <div class="gate">
        <app-logo [size]="72" />
        <button type="button" class="gate-button" #gateButton (click)="begin($event)">
          Cliquer pour jouer
        </button>
      </div>
    } @else if (visible()) {
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
  /** Avant le clic de départ : l'écran « Cliquer pour jouer », sans rien d'autre qui bouge ni qui sonne. */
  protected readonly gated = signal(false);
  protected readonly visible = signal(shouldPlay());
  private readonly audio = inject(AudioService);
  private readonly gateButton = viewChild<ElementRef<HTMLButtonElement>>('gateButton');
  protected readonly leaving = signal(false);
  private readonly timers: ReturnType<typeof setTimeout>[] = [];

  constructor() {
    // Le seul geste attendu : le focus est déjà sur le bouton, Entrée ou Espace suffisent
    effect(() => this.gateButton()?.nativeElement.focus());
    if (this.visible()) {
      remember();
      this.gated.set(true);
    }
    inject(DestroyRef).onDestroy(() => this.timers.forEach(clearTimeout));
  }

  /** Le clic de départ : l'audio est débloqué, l'ouverture démarre avec son son. */
  protected begin(event: Event): void {
    event.stopPropagation();
    this.audio.unlock();
    this.gated.set(false);
    this.audio.play('intro');
    this.timers.push(
      setTimeout(() => this.skip(), this.reduced ? REDUCED_DURATION_MS : DURATION_MS),
    );
  }

  /** Un clic, une touche ou la fin du temps : un dernier fondu, puis l'accueil. */
  protected skip(): void {
    if (this.gated() || !this.visible() || this.leaving()) {
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
