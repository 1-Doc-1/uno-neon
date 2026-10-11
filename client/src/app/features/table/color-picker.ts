import {
  afterNextRender,
  Component,
  DOCUMENT,
  ElementRef,
  inject,
  input,
  output,
  signal,
  viewChild,
} from '@angular/core';
import type { Color } from '../../protocol/generated/protocol';
import { COLOR_NAME, COLORS } from '../../ui/color-meta';
import { ColorSymbol } from '../../ui/color-symbol';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';

/** Le temps que la couleur choisie grossit et brille avant que le choix parte (rien en mouvement réduit). */
export const CHOSEN_MS = 520;
/** Les étincelles qui partent de la couleur choisie. */
const SPARKS = Array.from({ length: 14 }, (_, index) => index);
/** Marge entre la roue et le bord de l'écran. */
const EDGE_MARGIN = 8;

/**
 * Choix de la couleur d'un joker (SPEC §12.3) : une roue de quatre quarts de disque qui se déploie en cercle depuis la
 * pile de défausse, là où la carte va se poser. La roue ne sort jamais de l'écran ni d'elle-même : chaque quart est un
 * quart de disque posé dans son quadrant. Raccourcis clavier 1 à 4, Échap annule quand c'est permis. La couleur choisie
 * grossit, brille et lance des étincelles, puis le choix part ; en mouvement réduit, il part tout de suite.
 */
@Component({
  selector: 'app-color-picker',
  imports: [ClickSound, ColorSymbol, Button],
  host: { '(document:keydown)': 'onKey($event)', '(click)': 'onBackdrop($event)' },
  template: `
    <div class="backdrop">
      <div
        class="dialog"
        role="dialog"
        aria-modal="true"
        aria-label="Choisir une couleur"
        #dialog
        [style.left.px]="centre().x"
        [style.top.px]="centre().y"
      >
        <div class="wheel" [class.chosen-one]="chosen() !== null">
          @for (color of colors; track color; let index = $index) {
            <button
              type="button"
              [class]="'wedge q' + index + ' tint-' + color"
              [class.chosen]="chosen() === color"
              [class.faded]="chosen() !== null && chosen() !== color"
              [style.--i]="index"
              [attr.aria-label]="names[color]"
              [disabled]="chosen() !== null"
              (click)="choose(color)"
            >
              <span class="content">
                <app-color-symbol [color]="color" />
                <span class="name" aria-hidden="true">{{ names[color] }}</span>
                <kbd aria-hidden="true">{{ index + 1 }}</kbd>
              </span>
              @if (chosen() === color) {
                <span class="sparks" aria-hidden="true">
                  @for (spark of sparks; track spark) {
                    <i [style.--a.deg]="spark * (360 / sparks.length)"></i>
                  }
                </span>
              }
            </button>
          }
        </div>
        @if (cancellable() && chosen() === null) {
          <button appButton kind="ghost" class="cancel" (click)="cancelled.emit()">Annuler</button>
        }
      </div>
    </div>
  `,
  styles: `
    :host {
      --size: min(78vmin, 340px);
      position: fixed;
      inset: 0;
      z-index: var(--z-modal);
    }
    .backdrop {
      position: absolute;
      inset: 0;
      background: var(--scrim);
    }
    .dialog {
      position: absolute;
      transform: translate(-50%, -50%);
    }
    .wheel {
      position: relative;
      width: var(--size);
      height: var(--size);
      animation: wheel-open 380ms var(--ease-out) backwards;
    }
    .wedge {
      --r: calc(var(--size) / 2);
      position: absolute;
      width: calc(var(--r) - 3px);
      height: calc(var(--r) - 3px);
      display: grid;
      padding: 0;
      border: 2px solid currentColor;
      background: color-mix(in oklab, var(--card-body) 62%, currentColor);
      color: var(--game-red);
      box-shadow: var(--glow-sm);
      font-family: var(--font-display);
      font-weight: 700;
      cursor: pointer;
      // chaque quart apparaît à son tour, depuis le centre de la roue
      animation: wedge-open 360ms var(--ease-out) calc(var(--i) * 70ms) backwards;
      transition:
        transform 320ms var(--ease-out),
        opacity 320ms,
        filter 320ms,
        box-shadow 320ms;
    }
    .wedge:hover:not(:disabled),
    .wedge:focus-visible {
      filter: brightness(1.25);
      box-shadow: var(--glow-md);
    }
    .wedge:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: 2px;
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
    .tint-blue {
      color: var(--game-blue);
    }
    // Quatre quarts de disque, dans le sens des aiguilles d'une montre : rouge, jaune, vert, bleu
    .q0 {
      top: 0;
      left: 0;
      border-top-left-radius: 100%;
      transform-origin: 100% 100%;
      place-content: start;
      --dx: 14%;
      --dy: 14%;
    }
    .q1 {
      top: 0;
      right: 0;
      border-top-right-radius: 100%;
      transform-origin: 0 100%;
      --dx: -14%;
      --dy: 14%;
    }
    .q2 {
      bottom: 0;
      right: 0;
      border-bottom-right-radius: 100%;
      transform-origin: 0 0;
      --dx: -14%;
      --dy: -14%;
    }
    .q3 {
      bottom: 0;
      left: 0;
      border-bottom-left-radius: 100%;
      transform-origin: 100% 0;
      --dx: 14%;
      --dy: -14%;
    }
    .content {
      display: grid;
      justify-items: center;
      align-self: center;
      justify-self: center;
      gap: 2px;
      translate: var(--dx) var(--dy);
    }
    .content app-color-symbol {
      font-size: clamp(1.4rem, calc(var(--size) / 9), 2.2rem);
    }
    .name {
      color: var(--text);
      font-size: var(--fs-sm);
    }
    kbd {
      color: var(--text-dim);
      font-family: var(--font-mono);
      font-size: var(--fs-xs);
    }
    // La couleur choisie grossit et brille, les autres s'effacent
    .wedge.chosen {
      z-index: 2;
      transform: scale(1.32);
      box-shadow:
        0 0 0 3px currentColor,
        0 0 34px currentColor,
        0 0 90px color-mix(in oklab, currentColor 60%, transparent);
      filter: brightness(1.35);
    }
    .wedge.faded {
      opacity: 0;
      transform: scale(0.85);
    }
    .sparks {
      position: absolute;
      inset: 0;
      pointer-events: none;
    }
    .sparks i {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 8px;
      height: 8px;
      margin: -4px;
      border-radius: 50%;
      background: currentColor;
      box-shadow: 0 0 10px currentColor;
      animation: spark 520ms ease-out forwards;
    }
    .cancel {
      position: absolute;
      top: 100%;
      left: 50%;
      translate: -50% var(--space-3);
    }
    @keyframes wheel-open {
      from {
        opacity: 0;
        transform: scale(0.25) rotate(-90deg);
      }
    }
    @keyframes wedge-open {
      from {
        opacity: 0;
        transform: scale(0.1) rotate(-45deg);
      }
    }
    @keyframes spark {
      from {
        opacity: 1;
        transform: rotate(var(--a)) translateX(0) scale(1);
      }
      to {
        opacity: 0;
        transform: rotate(var(--a)) translateX(calc(var(--size) / 3.2)) scale(0.2);
      }
    }
    // Mouvement réduit : la roue est là d'un coup, la couleur choisie ne fait que se marquer
    @media (prefers-reduced-motion: reduce) {
      .wheel,
      .wedge {
        animation: none;
        transition: none;
      }
      .wedge.chosen {
        transform: none;
      }
      .sparks {
        display: none;
      }
    }
  `,
})
export class ColorPicker {
  /** Faux quand le choix est obligatoire (joker retourné en première carte). */
  readonly cancellable = input(true);
  readonly picked = output<Color>();
  readonly cancelled = output<void>();

  protected readonly colors = COLORS;
  protected readonly names = COLOR_NAME;
  protected readonly sparks = SPARKS;
  /** La couleur qu'on vient de choisir, le temps de l'animation. */
  protected readonly chosen = signal<Color | null>(null);
  /** Le centre de la roue, en pixels de l'écran : la pile de défausse, ramenée dans l'écran. */
  protected readonly centre = signal({ x: 0, y: 0 });

  private readonly document = inject(DOCUMENT);
  private readonly dialog = viewChild<ElementRef<HTMLElement>>('dialog');

  constructor() {
    this.centre.set(this.centreOn(null));
    afterNextRender(() => {
      const anchor = this.document.querySelector('[data-anchor="discard"]');
      this.centre.set(this.centreOn(anchor?.getBoundingClientRect() ?? null));
      this.dialog()?.nativeElement.querySelector<HTMLElement>('.wedge')?.focus();
    });
  }

  protected choose(color: Color): void {
    if (this.chosen() !== null) {
      return;
    }
    this.chosen.set(color);
    const win = this.document.defaultView;
    const reduced = win?.matchMedia?.('(prefers-reduced-motion: reduce)').matches === true;
    if (reduced || win === null) {
      this.picked.emit(color);
    } else {
      win.setTimeout(() => this.picked.emit(color), CHOSEN_MS);
    }
  }

  protected onKey(event: KeyboardEvent): void {
    const color = COLORS[Number(event.key) - 1];
    if (color) {
      this.choose(color);
    } else if (event.key === 'Escape' && this.cancellable() && this.chosen() === null) {
      this.cancelled.emit();
    }
  }

  protected onBackdrop(event: MouseEvent): void {
    const outside = (event.target as HTMLElement).classList.contains('backdrop');
    if (outside && this.cancellable() && this.chosen() === null) {
      this.cancelled.emit();
    }
  }

  /** Le centre de la défausse (ou de l'écran à défaut), éloigné des bords pour que toute la roue reste visible. */
  private centreOn(rect: DOMRect | null): { x: number; y: number } {
    const win = this.document.defaultView;
    const width = win?.innerWidth ?? 0;
    const height = win?.innerHeight ?? 0;
    const size = Math.min(0.78 * Math.min(width, height), 340);
    const half = size / 2 + EDGE_MARGIN;
    const wanted = rect
      ? { x: rect.left + rect.width / 2, y: rect.top + rect.height / 2 }
      : { x: width / 2, y: height / 2 };
    // Le bouton « Annuler » prend un peu de place sous la roue
    const below = this.cancellable() ? 56 : 0;
    return {
      x: Math.max(half, Math.min(width - half, wanted.x)),
      y: Math.max(half, Math.min(height - half - below, wanted.y)),
    };
  }
}
