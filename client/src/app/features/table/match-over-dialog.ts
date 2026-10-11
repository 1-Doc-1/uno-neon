import { Component, computed, input, output, signal } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Avatar } from '../../ui/avatar';
import { Modal } from '../../ui/modal';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';
import { MatchEndCinematic } from './match-end-cinematic';

/**
 * Fin de partie : d'abord la cinématique du podium (passable), puis le vainqueur, le classement et les choix. Plus de relance automatique : l'hôte choisit entre une revanche (mêmes
 * joueurs, mêmes réglages) et le retour au salon ; les autres attendent sa décision (ADR 0037).
 */
@Component({
  selector: 'app-match-over-dialog',
  imports: [ClickSound, Avatar, Modal, Button, MatchEndCinematic],
  template: `
    @if (cinematic()) {
      <app-match-end-cinematic
        [players]="players()"
        [winnerId]="winnerId()"
        [meId]="meId()"
        (finished)="cinematic.set(false)"
      />
    } @else {
      <app-modal label="Fin de la partie">
        <div class="body">
          <h2>{{ winner()?.nickname }} remporte la partie !</h2>
          <ol class="ranking">
            @for (player of ranking(); track player.playerId) {
              <li>
                <app-avatar [avatarId]="player.avatarId" />
                <span class="name">{{ player.nickname }}</span>
                <span class="score">{{ player.score }} pts</span>
              </li>
            }
          </ol>
          @if (isHost()) {
            <div class="actions">
              <button appButton kind="primary" (click)="rematch.emit()">Revanche</button>
              <button appButton kind="neutral" (click)="backToLobby.emit()">Retour au salon</button>
            </div>
          } @else {
            <p class="waiting" role="status">En attente de l’hôte…</p>
          }
          <button appButton kind="ghost" (click)="left.emit()">Quitter</button>
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
      color: var(--text);
    }
    .ranking {
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
      align-items: center;
      gap: var(--space-3);
      padding: var(--space-2) var(--space-3);
      background: var(--field);
      border-radius: var(--radius-md);
    }
    .name {
      flex: 1;
      font-weight: 700;
    }
    .score {
      font-family: var(--font-display);
      font-weight: 700;
    }
    .actions {
      display: flex;
      flex-wrap: wrap;
      gap: var(--space-3);
      justify-content: center;
    }
    .waiting {
      color: var(--text-dim);
    }
  `,
})
export class MatchOverDialog {
  readonly players = input.required<readonly SeatView[]>();
  readonly winnerId = input.required<string | null>();
  /** Mon identifiant : le son de la cinématique dit si j'ai gagné. */
  readonly meId = input.required<string>();
  /** Seul l'hôte relance ou ramène au salon. */
  readonly isHost = input(false);
  readonly rematch = output<void>();
  readonly backToLobby = output<void>();
  readonly left = output<void>();

  protected readonly cinematic = signal(true);
  protected readonly ranking = computed(() =>
    [...this.players()].sort((a, b) => b.score - a.score),
  );
  protected readonly winner = computed(() =>
    this.players().find((p) => p.playerId === this.winnerId()),
  );
}
