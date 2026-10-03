import { Component, inject } from '@angular/core';
import { GameStore } from '../state/game-store';

/** Messages brefs (erreurs du serveur traduites en français), annoncés aux lecteurs d'écran. */
@Component({
  selector: 'app-toasts',
  template: `
    <div class="stack" role="status" aria-live="polite">
      @for (notice of store.notices(); track notice.id) {
        <button type="button" class="glass strong toast" (click)="store.dismissNotice(notice.id)">
          {{ notice.text }}
        </button>
      }
    </div>
  `,
  styles: `
    .stack {
      position: fixed;
      z-index: var(--z-toast);
      left: 50%;
      bottom: var(--space-4);
      display: grid;
      gap: var(--space-2);
      width: min(92vw, 420px);
      transform: translateX(-50%);
    }
    .toast {
      padding: var(--space-3) var(--space-4);
      border-color: var(--neon-pink);
      color: var(--text-primary);
      text-align: left;
      cursor: pointer;
      animation: toast-in var(--dur-base) var(--ease-out);
    }
    @keyframes toast-in {
      from {
        opacity: 0;
        transform: translateY(12px);
      }
    }
  `,
})
export class Toasts {
  protected readonly store = inject(GameStore);
}
