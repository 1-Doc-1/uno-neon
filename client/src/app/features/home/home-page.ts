import { Component, computed, effect, inject, signal } from '@angular/core';
import { form, FormField, pattern, required } from '@angular/forms/signals';
import { Router } from '@angular/router';
import { SessionService } from '../../core/session.service';
import { GameStore } from '../../state/game-store';
import { Button } from '../../ui/button';
import { Icon } from '../../ui/icon';
import { Logo } from '../../ui/logo';
import { SoundControl } from '../../ui/sound-control';
import { isRoomCode, NICKNAME_PATTERN } from '../../core/validation';
import { ClickSound } from '../../ui/click-sound';

/**
 * Accueil : un pseudo, puis trois grands choix (SPEC §12.1) : jouer tout de suite contre des bots, créer un salon, ou
 * rejoindre celui d'un ami par son code.
 */
@Component({
  selector: 'app-home-page',
  imports: [ClickSound, FormField, Button, Icon, Logo, SoundControl],
  templateUrl: './home-page.html',
  styleUrl: './home-page.scss',
})
export class HomePage {
  protected readonly store = inject(GameStore);
  private readonly router = inject(Router);
  private readonly session = inject(SessionService);

  protected readonly model = signal({ nickname: inject(SessionService).nickname(), code: '' });
  protected readonly form = form(this.model, (path) => {
    required(path.nickname);
    pattern(path.nickname, NICKNAME_PATTERN);
  });

  protected readonly busy = signal(false);
  /** Choisir ses bots ne demande pas encore le serveur : seul le pseudo doit être valide. */
  protected readonly canStart = computed(() => this.form.nickname().valid());
  protected readonly canCreate = computed(
    () => this.store.ready() && !this.busy() && this.form.nickname().valid(),
  );
  protected readonly canJoin = computed(() => this.canCreate() && isRoomCode(this.model().code));

  constructor() {
    effect(() => {
      const code = this.store.roomCode();
      if (code) {
        void this.router.navigate(['/r', code]);
      }
    });
  }

  protected playAgainstBots(): void {
    this.session.saveNickname(this.nickname());
    void this.router.navigate(['/bots']);
  }

  protected create(): Promise<void> {
    return this.run(() => this.store.createRoom(this.nickname()));
  }

  protected join(): Promise<void> {
    return this.run(() =>
      this.store.joinRoom(this.model().code.trim().toUpperCase(), this.nickname()),
    );
  }

  private nickname(): string {
    return this.model().nickname.trim();
  }

  private async run(action: () => Promise<boolean>): Promise<void> {
    this.busy.set(true);
    try {
      await action();
    } finally {
      this.busy.set(false);
    }
  }
}
