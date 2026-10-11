import { Component, input, output, signal } from '@angular/core';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';
import { Icon } from '../../ui/icon';
import { Modal } from '../../ui/modal';

/**
 * Le menu de la partie : quitter, et pour l'hôte « Retour au salon » (la partie est abandonnée, sans score), après une
 * confirmation (ADR 0037).
 */
@Component({
  selector: 'app-game-menu',
  imports: [ClickSound, Button, Icon, Modal],
  template: `
    <button type="button" class="open" aria-label="Menu de la partie" (click)="step.set('menu')">
      <app-icon name="door" />
      <span class="text">Menu</span>
    </button>
    @switch (step()) {
      @case ('menu') {
        <app-modal label="Menu de la partie" [dismissible]="true" (dismissed)="step.set(null)">
          <div class="body">
            <h2>Menu de la partie</h2>
            <div class="actions">
              @if (isHost()) {
                <button appButton kind="neutral" (click)="step.set('confirm')">
                  Retour au salon
                </button>
              }
              <button appButton kind="danger" (click)="left.emit()">Quitter la partie</button>
              <button appButton kind="ghost" (click)="step.set(null)">Continuer à jouer</button>
            </div>
          </div>
        </app-modal>
      }
      @case ('confirm') {
        <app-modal label="Abandonner la partie ?" [dismissible]="true" (dismissed)="step.set(null)">
          <div class="body">
            <h2>Abandonner la partie ?</h2>
            <p>Tout le monde retourne au salon. La partie est perdue, sans score.</p>
            <div class="actions">
              <button appButton kind="danger" (click)="confirmed()">Retour au salon</button>
              <button appButton kind="ghost" (click)="step.set(null)">Annuler</button>
            </div>
          </div>
        </app-modal>
      }
    }
  `,
  styles: `
    :host {
      display: block;
    }
    .open {
      display: inline-flex;
      align-items: center;
      gap: var(--space-2);
      min-height: 44px;
      padding: var(--space-2) var(--space-3);
      border: 1px solid var(--panel-solid);
      border-radius: var(--radius-pill);
      background: var(--panel-solid);
      color: var(--text);
      font-family: var(--font-display);
      font-weight: 700;
      cursor: pointer;
    }
    .open:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: 2px;
    }
    app-icon {
      width: 1.2em;
      height: 1.2em;
    }
    .body {
      display: grid;
      gap: var(--space-3);
      justify-items: center;
      text-align: center;
    }
    h2 {
      font-size: var(--fs-lg);
    }
    p {
      color: var(--text-dim);
    }
    .actions {
      display: grid;
      gap: var(--space-2);
      width: 100%;
    }
    @media (max-width: 639px) {
      .text {
        display: none;
      }
    }
  `,
})
export class GameMenu {
  readonly isHost = input(false);
  readonly backToLobby = output<void>();
  readonly left = output<void>();

  protected readonly step = signal<'menu' | 'confirm' | null>(null);

  protected confirmed(): void {
    this.step.set(null);
    this.backToLobby.emit();
  }
}
