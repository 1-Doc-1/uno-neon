import { Component, input, output } from '@angular/core';
import type { PenaltyResponseOptions } from '../../protocol/generated/protocol';
import { NeonButton } from '../../ui/neon-button';

/** Réponse à une pénalité : piocher, contester un +4, ou empiler en jouant une carte de la main. */
@Component({
  selector: 'app-penalty-bar',
  imports: [NeonButton],
  template: `
    <section class="glass bar" aria-label="Pénalité">
      <p>
        <strong>Tu dois piocher {{ options().amount }} cartes.</strong>
        @if (options().canChallenge) {
          Si tu penses que le Joker +4 est un bluff, tu peux le contester : s’il était interdit,
          c’est son poseur qui pioche ; sinon tu en piocheras 2 de plus.
        }
        @if (options().canStack) {
          Tu peux aussi empiler une carte jouable.
        }
      </p>
      <div class="actions">
        <button appNeonButton tone="red" (click)="accepted.emit()">
          Piocher {{ options().amount }}
        </button>
        @if (options().canChallenge) {
          <button appNeonButton variant="outline" tone="yellow" (click)="challenged.emit()">
            Contester
          </button>
        }
      </div>
    </section>
  `,
  styles: `
    .bar {
      display: grid;
      gap: var(--space-3);
      padding: var(--space-3) var(--space-4);
      border-color: var(--neon-red);
    }
    p {
      margin: 0;
    }
    .actions {
      display: flex;
      flex-wrap: wrap;
      gap: var(--space-3);
    }
  `,
})
export class PenaltyBar {
  readonly options = input.required<PenaltyResponseOptions>();
  readonly accepted = output<void>();
  readonly challenged = output<void>();
}
