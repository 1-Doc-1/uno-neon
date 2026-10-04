import { Component, computed, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { NavigationEnd, Router } from '@angular/router';
import { filter, map } from 'rxjs';
import { GameStore } from '../state/game-store';

/**
 * Panneau « Connexion perdue » affiché par-dessus la page tant que le serveur ne répond pas (SPEC §12.3).
 * Les pages `/dev` n'ont pas de WebSocket : l'absence de connexion y est normale, le panneau n'y apparaît pas.
 */
@Component({
  selector: 'app-connection-banner',
  template: `
    @if (lost()) {
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
  private readonly router = inject(Router);
  private readonly url = toSignal(
    this.router.events.pipe(
      filter((event) => event instanceof NavigationEnd),
      map((event) => event.urlAfterRedirects),
    ),
    { initialValue: this.router.url },
  );
  private readonly onDevPage = computed(() => /^\/dev(\/|\?|$)/.test(this.url()));
  protected readonly lost = computed(
    () =>
      !this.onDevPage() &&
      (this.store.connection() === 'reconnecting' || this.store.connection() === 'closed'),
  );
}
