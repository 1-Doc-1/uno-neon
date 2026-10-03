import { Component, computed, input, output } from '@angular/core';
import type { Card, Color, PenaltyResponseOptions } from '../../protocol/generated/protocol';
import { Button } from '../../ui/button';
import { CardFace } from '../../ui/card';
import { Modal } from '../../ui/modal';

/** Réponse à une pénalité, façon jeu de cartes : la carte jouée contre moi, puis « Contester » ou « Accepter ». */
@Component({
  selector: 'app-challenge-dialog',
  imports: [Modal, CardFace, Button],
  template: `
    <app-modal [label]="title()">
      <div class="body">
        <h2>{{ title() }}</h2>
        <app-card class="big" [card]="card()" [chosenColor]="color()" [top]="true" />
        <p class="rule">
          @if (options().canChallenge) {
            Contester : si le +4 était un bluff (son poseur avait une carte de la couleur active),
            c’est lui qui pioche 4 ; sinon tu en piocheras 6.
          } @else {
            Tu dois piocher {{ options().amount }} cartes.
          }
        </p>
        <div class="actions">
          @if (options().canChallenge) {
            <button appButton kind="danger" (click)="challenged.emit()">Contester</button>
          }
          <button appButton kind="neutral" (click)="accepted.emit()">
            Accepter, piocher {{ options().amount }}
          </button>
        </div>
      </div>
    </app-modal>
  `,
  styles: `
    .body {
      display: grid;
      justify-items: center;
      gap: var(--space-4);
      text-align: center;
    }
    h2 {
      font-size: var(--fs-xl);
    }
    .big {
      --card-w: 120px;
    }
    .rule {
      max-width: 38ch;
      color: var(--text-dim);
      font-size: var(--fs-sm);
    }
    .actions {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: var(--space-3);
    }
  `,
})
export class ChallengeDialog {
  /** La carte qui vient d'être posée contre moi (le dessus de la défausse). */
  readonly card = input.required<Card>();
  readonly color = input.required<Color | null>();
  readonly options = input.required<PenaltyResponseOptions>();
  readonly accepted = output<void>();
  readonly challenged = output<void>();

  protected readonly title = computed(() =>
    this.card().rank === 'wildDrawFour' ? 'Joker +4 contre toi !' : '+2 contre toi !',
  );
}
