import { Component, computed, input, output } from '@angular/core';
import type { Card, RoundResult, SeatView } from '../../protocol/generated/protocol';
import { CardFace } from '../../ui/card';
import { Modal } from '../../ui/modal';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';

/** Fin de manche : vainqueur, points, mains révélées, scores ; chacun valide pour enchaîner (SPEC §12.3). */
@Component({
  selector: 'app-round-over-dialog',
  imports: [ClickSound, Modal, CardFace, Button],
  template: `
    <app-modal label="Fin de la manche">
      <div class="body">
        <h2>{{ winnerName() }} gagne la manche !</h2>
        <p class="points">+{{ result().points }} points</p>

        <ul class="players">
          @for (player of players(); track player.playerId) {
            <li>
              <span class="name">
                {{ player.nickname }}
                @if (player.isReadyForNextRound) {
                  <span class="ready">prêt</span>
                }
              </span>
              <span class="score">{{ player.score }} pts</span>
              <span class="hand">
                @for (card of handOf(player.playerId); track card.id) {
                  <app-card [card]="card" />
                }
              </span>
            </li>
          }
        </ul>

        <button appButton kind="primary" [disabled]="meReady()" (click)="next.emit()">
          {{ meReady() ? 'En attente des autres…' : 'Manche suivante' }}
        </button>
        @if (secondsLeft() !== null) {
          <p class="muted">Départ automatique dans {{ secondsLeft() }} s</p>
        }
      </div>
    </app-modal>
  `,
  styles: `
    .body {
      display: grid;
      gap: var(--space-3);
      justify-items: center;
      text-align: center;
    }
    h2 {
      font-size: var(--fs-lg);
      color: var(--text);
    }
    p {
      margin: 0;
    }
    .points {
      font-family: var(--font-display);
      font-size: var(--fs-xl);
      font-weight: 900;
      color: var(--game-green);
    }
    .players {
      display: grid;
      gap: var(--space-2);
      width: 100%;
      margin: 0;
      padding: 0;
      list-style: none;
      text-align: left;
    }
    li {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: var(--space-2) var(--space-3);
      padding: var(--space-2) var(--space-3);
      background: var(--field);
      border-radius: var(--radius-md);
    }
    .name {
      flex: 1;
      font-weight: 700;
    }
    .ready {
      color: var(--game-green);
      font-size: var(--fs-xs);
    }
    .score {
      font-family: var(--font-display);
      font-weight: 700;
    }
    .hand {
      display: flex;
      flex-basis: 100%;
      flex-wrap: wrap;
      gap: var(--space-1);
      --card-w: 36px;
    }
    .muted {
      color: var(--text-dim);
    }
  `,
})
export class RoundOverDialog {
  readonly result = input.required<RoundResult>();
  readonly players = input.required<readonly SeatView[]>();
  readonly meReady = input.required<boolean>();
  readonly secondsLeft = input.required<number | null>();
  readonly next = output<void>();

  protected readonly winnerName = computed(
    () => this.players().find((p) => p.playerId === this.result().winnerId)?.nickname ?? '',
  );

  protected handOf(playerId: string): readonly Card[] {
    return this.result().revealedHands[playerId] ?? [];
  }
}
