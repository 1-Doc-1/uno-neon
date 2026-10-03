import { Component, inject } from '@angular/core';
import { GameStore } from '../state/game-store';

/** Panneau « Connexion perdue » affiché par-dessus la page tant que le serveur ne répond pas (SPEC §12.3). */
@Component({
  selector: 'app-connection-banner',
  template: `
    @if (store.connection() === 'reconnecting' || store.connection() === 'closed') {
      <div class="veil" role="alert">
        <div class="panel box">
          <h2>Connexion perdue</h2>
          @if (store.connection() === 'reconnecting') {
            <p>Reconnexion… (tentative {{ store.reconnectAttempt() }})</p>
          } @else {
            <p>La connexion a été fermée. Recharge la page pour reprendre.</p>
          }
        </div>
      </div>
    }
  `,
  styles: `
    .veil {
      position: fixed;
      inset: 0;
      z-index: var(--z-modal);
      display: grid;
      place-items: center;
      background: rgb(0 0 0 / 0.6);
    }
    .box {
      display: grid;
      gap: var(--space-2);
      padding: var(--space-5);
      text-align: center;
    }
    h2 {
      font-size: var(--fs-lg);
      color: var(--text);
    }
  `,
})
export class ConnectionBanner {
  protected readonly store = inject(GameStore);
}
