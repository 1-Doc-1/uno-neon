import { Component, computed, effect, inject, signal } from '@angular/core';
import { form, FormField, pattern, required } from '@angular/forms/signals';
import { Router } from '@angular/router';
import { SessionService } from '../../core/session.service';
import { GameStore } from '../../state/game-store';
import { ThemeService } from '../../core/theme.service';
import { Button } from '../../ui/button';
import { Icon } from '../../ui/icon';
import { Logo } from '../../ui/logo';
import { isRoomCode, NICKNAME_PATTERN } from '../../core/validation';

/** Accueil : choisir un pseudo, puis créer un salon ou rejoindre celui d'un ami par son code. */
@Component({
  selector: 'app-home-page',
  imports: [FormField, Button, Icon, Logo],
  templateUrl: './home-page.html',
  styleUrl: './home-page.scss',
})
export class HomePage {
  protected readonly store = inject(GameStore);
  protected readonly theme = inject(ThemeService);
  private readonly router = inject(Router);

  protected readonly model = signal({ nickname: inject(SessionService).nickname(), code: '' });
  protected readonly form = form(this.model, (path) => {
    required(path.nickname);
    pattern(path.nickname, NICKNAME_PATTERN);
  });

  protected readonly busy = signal(false);
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
