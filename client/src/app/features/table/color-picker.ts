import { Component, input, output } from '@angular/core';
import type { Color } from '../../protocol/generated/protocol';
import { COLOR_NAME, COLORS } from '../../ui/color-meta';
import { ColorSymbol } from '../../ui/color-symbol';
import { Modal } from '../../ui/modal';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';

/** Choix de la couleur d'un joker : quatre grandes tuiles, raccourcis clavier 1 à 4 (SPEC §12.3). */
@Component({
  selector: 'app-color-picker',
  imports: [ClickSound, Modal, ColorSymbol, Button],
  host: { '(document:keydown)': 'onKey($event)' },
  template: `
    <app-modal
      label="Choisir une couleur"
      [dismissible]="cancellable()"
      (dismissed)="cancelled.emit()"
    >
      <div class="body">
        <h2>Choisis une couleur</h2>
        <div class="tiles">
          @for (color of colors; track color; let index = $index) {
            <button type="button" [class]="'tile tint-' + color" (click)="picked.emit(color)">
              <app-color-symbol [color]="color" />
              <span>{{ names[color] }}</span>
              <kbd aria-hidden="true">{{ index + 1 }}</kbd>
            </button>
          }
        </div>
        @if (cancellable()) {
          <button appButton kind="ghost" (click)="cancelled.emit()">Annuler</button>
        }
      </div>
    </app-modal>
  `,
  styles: `
    .body {
      display: grid;
      gap: var(--space-4);
      justify-items: center;
    }
    h2 {
      font-size: var(--fs-lg);
    }
    .tiles {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: var(--space-3);
      width: 100%;
    }
    .tile {
      display: grid;
      justify-items: center;
      gap: var(--space-2);
      min-height: 96px;
      padding: var(--space-3);
      border: 2px solid currentColor;
      border-radius: var(--radius-md);
      background: color-mix(in oklab, var(--card-body) 66%, currentColor);
      box-shadow: var(--glow-sm);
      font-family: var(--font-display);
      font-weight: 700;
      font-size: var(--fs-base);
      cursor: pointer;
      transition: transform var(--dur-fast) var(--ease-out);
    }
    .tile:hover {
      transform: translateY(-3px);
      box-shadow: var(--glow-md);
    }
    .tile app-color-symbol {
      font-size: 2rem;
    }
    .tile span {
      color: var(--text);
    }
    kbd {
      color: var(--text-dim);
      font-family: var(--font-mono);
      font-size: var(--fs-xs);
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

  protected onKey(event: KeyboardEvent): void {
    const color = COLORS[Number(event.key) - 1];
    if (color) {
      this.picked.emit(color);
    }
  }
}
