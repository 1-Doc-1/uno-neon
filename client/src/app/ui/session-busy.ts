import { Component, computed, inject } from '@angular/core';
import { GameStore } from '../state/game-store';
import { Button } from './button';
import { ClickSound } from './click-sound';
import { Modal } from './modal';

/**
 * La session est ouverte dans un autre onglet (ou cet onglet a été repris ailleurs) : on le dit, et un bouton la
 * reprend ici en fermant l'autre (ADR 0036).
 */
@Component({
  selector: 'app-session-busy',
  imports: [ClickSound, Button, Modal],
  template: `
    @if (visible()) {
      <app-modal label="Partie ouverte dans un autre onglet">
        <div class="body">
          <h2>Partie ouverte dans un autre onglet</h2>
          <p>
            Ce navigateur joue déjà dans un autre onglet. Tu peux reprendre la partie ici : l’autre
            onglet sera fermé.
          </p>
          <button appButton kind="primary" (click)="store.takeOver()">Jouer ici</button>
        </div>
      </app-modal>
    }
  `,
  styles: `
    .body {
      display: grid;
      gap: var(--space-4);
      justify-items: center;
      text-align: center;
    }
    h2 {
      font-size: var(--fs-xl);
    }
    p {
      color: var(--text-dim);
    }
  `,
})
export class SessionBusy {
  protected readonly store = inject(GameStore);
  protected readonly visible = computed(() => this.store.busy() || this.store.takenOver());
}
