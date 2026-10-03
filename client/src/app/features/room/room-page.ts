import { Component, computed, effect, inject, input, signal } from '@angular/core';
import { form, FormField, pattern, required } from '@angular/forms/signals';
import { Router } from '@angular/router';
import { NICKNAME_PATTERN } from '../../core/validation';
import { SessionService } from '../../core/session.service';
import { GameStore } from '../../state/game-store';
import { NeonButton } from '../../ui/neon-button';
import { Lobby } from './lobby';

/** Page `/r/:code` : salon (lobby) ou table de jeu selon l'état, et entrée par lien partagé. */
@Component({
  selector: 'app-room-page',
  imports: [FormField, NeonButton, Lobby],
  templateUrl: './room-page.html',
  styleUrl: './room-page.scss',
})
export class RoomPage {
  protected readonly store = inject(GameStore);
  private readonly router = inject(Router);

  readonly code = input.required<string>();

  protected readonly model = signal({ nickname: inject(SessionService).nickname() });
  protected readonly form = form(this.model, (path) => {
    required(path.nickname);
    pattern(path.nickname, NICKNAME_PATTERN);
  });
  protected readonly canJoin = computed(() => this.store.ready() && this.form.nickname().valid());

  constructor() {
    effect(() => {
      const joined = this.store.roomCode();
      if (joined && joined !== this.code().toUpperCase()) {
        void this.router.navigate(['/r', joined]);
      }
    });
  }

  protected join(): Promise<boolean> {
    return this.store.joinRoom(this.code().toUpperCase(), this.model().nickname.trim());
  }

  protected goHome(): void {
    this.store.acknowledgeClosed();
    void this.router.navigate(['/']);
  }
}
