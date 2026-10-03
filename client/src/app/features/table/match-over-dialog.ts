import { Component, computed, input, output } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Modal } from '../../ui/modal';
import { Button } from '../../ui/button';

/** Fin de partie : vainqueur et classement. (La revanche viendra après le MVP, étape 4.3b.) */
@Component({
  selector: 'app-match-over-dialog',
  imports: [Modal, Button],
  template: `
    <app-modal label="Fin de la partie">
      <div class="body">
        <h2>{{ winner()?.nickname }} remporte la partie !</h2>
        <ol class="ranking">
          @for (player of ranking(); track player.playerId) {
            <li>
              <span class="name">{{ player.nickname }}</span>
              <span class="score">{{ player.score }} pts</span>
            </li>
          }
        </ol>
        <button appButton kind="primary" (click)="left.emit()">Quitter</button>
      </div>
    </app-modal>
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
      color: var(--text);
    }
    .ranking {
      display: grid;
      gap: var(--space-2);
      width: 100%;
      margin: 0;
      padding: 0;
      list-style-position: inside;
      text-align: left;
    }
    li {
      padding: var(--space-2) var(--space-3);
      background: var(--field);
      border-radius: var(--radius-md);
    }
    .name {
      font-weight: 700;
    }
    .score {
      float: right;
      font-family: var(--font-display);
      font-weight: 700;
    }
  `,
})
export class MatchOverDialog {
  readonly players = input.required<readonly SeatView[]>();
  readonly winnerId = input.required<string | null>();
  readonly left = output<void>();

  protected readonly ranking = computed(() =>
    [...this.players()].sort((a, b) => b.score - a.score),
  );
  protected readonly winner = computed(() =>
    this.players().find((p) => p.playerId === this.winnerId()),
  );
}
