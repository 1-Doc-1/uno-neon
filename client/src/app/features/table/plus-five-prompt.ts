import { Component, input, output } from '@angular/core';
import type { PenaltyResponseOptions } from '../../protocol/generated/protocol';
import { Button } from '../../ui/button';

/**
 * Un Joker +5 te vise : accepter (piocher le total), ou répliquer en posant un Joker +5 de ta main (on clique sur la
 * carte, comme pour la jouer). Pas modale : la main reste utilisable pour répliquer.
 */
@Component({
  selector: 'app-plus-five-prompt',
  imports: [Button],
  template: `
    <section class="panel" aria-label="Un Joker +5 te vise">
      <h2>+5 contre toi !</h2>
      <p>Tu dois piocher {{ options().amount }} cartes.</p>
      @if (options().canStack) {
        <p class="reply">Ou réponds avec ton Joker +5 : choisis-le dans ta main.</p>
      }
      <button appButton kind="neutral" [disabled]="waiting()" (click)="accepted.emit()">
        Accepter, piocher {{ options().amount }}
      </button>
    </section>
  `,
  styles: `
    :host {
      position: fixed;
      top: 38%;
      left: 50%;
      translate: -50% -50%;
      z-index: var(--z-modal);
      width: min(92vw, 360px);
    }
    section {
      display: grid;
      gap: var(--space-2);
      justify-items: center;
      padding: var(--space-4);
      border: 1px solid var(--gold-edge);
      text-align: center;
      box-shadow:
        var(--elevation),
        0 0 18px color-mix(in oklab, var(--gold-edge) 45%, transparent);
    }
    h2 {
      color: var(--gold-edge);
      font-size: var(--fs-lg);
    }
    .reply {
      color: var(--text-dim);
      font-size: var(--fs-sm);
    }
  `,
})
export class PlusFivePrompt {
  readonly options = input.required<PenaltyResponseOptions>();
  /** L'effet qui m'a visé est encore montré : le serveur n'accepte pas encore ma réponse (ADR 0027). */
  readonly waiting = input(false);
  readonly accepted = output<void>();
}
