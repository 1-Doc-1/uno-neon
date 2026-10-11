import { Component, computed, inject, input, output, signal } from '@angular/core';
import { GameStore } from '../../state/game-store';
import { Avatar } from '../../ui/avatar';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';
import { Modal } from '../../ui/modal';

/**
 * Le menu d'un joueur (clic sur son profil, au salon comme à la table) : masquer ses messages pour soi et, pour l'hôte,
 * l'exclure, après une confirmation (ADR 0037). C'est le serveur qui décide : seul l'hôte peut exclure.
 */
@Component({
  selector: 'app-player-menu',
  imports: [ClickSound, Avatar, Button, Modal],
  template: `
    @if (player(); as p) {
      <app-modal
        [label]="'Options de ' + p.nickname"
        [dismissible]="true"
        (dismissed)="closed.emit()"
      >
        <div class="body">
          @if (confirming()) {
            <h2>Exclure {{ p.nickname }} ?</h2>
            <p>Il quittera le salon{{ inGame() ? ' et la partie en cours' : '' }}.</p>
            <div class="actions">
              <button appButton kind="danger" (click)="kick()">Exclure</button>
              <button appButton kind="neutral" (click)="confirming.set(false)">Annuler</button>
            </div>
          } @else {
            <app-avatar [avatarId]="p.avatarId" class="big" />
            <h2>{{ p.nickname }}</h2>
            <div class="actions">
              @if (canMute()) {
                <button appButton kind="neutral" (click)="toggleMute()">
                  {{ muted() ? 'Réafficher ses messages' : 'Masquer ce joueur' }}
                </button>
              }
              @if (canKick()) {
                <button appButton kind="danger" (click)="confirming.set(true)">Exclure</button>
              }
              <button appButton kind="ghost" (click)="closed.emit()">Fermer</button>
            </div>
          }
        </div>
      </app-modal>
    }
  `,
  styles: `
    .body {
      display: grid;
      gap: var(--space-3);
      justify-items: center;
      text-align: center;
    }
    .big {
      font-size: 2.2rem;
    }
    h2 {
      font-size: var(--fs-lg);
    }
    p {
      color: var(--text-dim);
    }
    .actions {
      display: grid;
      gap: var(--space-2);
      width: 100%;
    }
  `,
})
export class PlayerMenu {
  protected readonly store = inject(GameStore);

  /** Le joueur dont on ouvre le menu. */
  readonly playerId = input.required<string>();
  readonly closed = output<void>();

  protected readonly confirming = signal(false);
  protected readonly player = computed(() =>
    this.store.room()?.players.find((p) => p.playerId === this.playerId()),
  );
  protected readonly isMe = computed(() => this.playerId() === this.store.playerId());
  protected readonly inGame = computed(() => this.store.room()?.phase !== 'lobby');
  protected readonly canKick = computed(() => this.store.isHost() && !this.isMe());
  /** On ne masque ni soi-même ni un bot (les bots n'écrivent pas). */
  protected readonly canMute = computed(() => !this.isMe() && !this.player()?.isBot);
  protected readonly muted = computed(() => this.store.mutedPlayers().has(this.playerId()));

  protected async kick(): Promise<void> {
    await this.store.kick(this.playerId());
    this.closed.emit();
  }

  protected toggleMute(): void {
    this.store.setMuted(this.playerId(), !this.muted());
    this.closed.emit();
  }
}
