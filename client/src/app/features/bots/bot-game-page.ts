import { Component, computed, effect, inject, signal } from '@angular/core';
import { Router } from '@angular/router';
import { SessionService } from '../../core/session.service';
import { NICKNAME_PATTERN } from '../../core/validation';
import type { BotLevel, RoomSettings, RoomSettingsPatch } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { Button } from '../../ui/button';
import { Icon } from '../../ui/icon';
import { Logo } from '../../ui/logo';
import { SegmentOption, Segmented } from '../../ui/segmented';
import { BOT_LEVELS } from '../room/bot-levels';
import { DEFAULT_SETTINGS } from '../room/default-settings';
import { GameSettings } from '../room/game-settings';

const BOT_COUNTS: readonly SegmentOption<number>[] = [1, 2, 3, 4, 5].map((count) => ({
  value: count,
  label: String(count),
}));

/**
 * « Jouer contre des bots » (SPEC §12.1) : le nombre de bots, leur niveau et les mêmes réglages de partie que le salon,
 * puis la partie démarre directement, sans salon d'attente.
 */
@Component({
  selector: 'app-bot-game-page',
  imports: [Button, GameSettings, Icon, Logo, Segmented],
  templateUrl: './bot-game-page.html',
  styleUrl: './bot-game-page.scss',
})
export class BotGamePage {
  protected readonly store = inject(GameStore);
  private readonly router = inject(Router);
  protected readonly nickname = inject(SessionService).nickname().trim();

  protected readonly botCounts = BOT_COUNTS;
  protected readonly botLevels = BOT_LEVELS;
  protected readonly botCount = signal(3);
  protected readonly level = signal<BotLevel>('normal');
  protected readonly settings = signal<RoomSettings>(DEFAULT_SETTINGS);
  protected readonly busy = signal(false);
  protected readonly canPlay = computed(
    () => this.store.ready() && !this.busy() && NICKNAME_PATTERN.test(this.nickname),
  );

  constructor() {
    // Sans pseudo valide (page ouverte directement), on repart de l'accueil
    if (!NICKNAME_PATTERN.test(this.nickname)) {
      void this.router.navigate(['/']);
    }
    effect(() => {
      const code = this.store.roomCode();
      if (code) {
        void this.router.navigate(['/r', code]);
      }
    });
  }

  protected change(patch: RoomSettingsPatch): void {
    this.settings.update((current) => ({ ...current, ...patch }));
  }

  protected async play(): Promise<void> {
    this.busy.set(true);
    try {
      // Le serveur fixe le nombre de places : celui des bots, plus le joueur
      const patch: RoomSettingsPatch = { ...this.settings() };
      delete patch.maxPlayers;
      await this.store.createBotGame(this.nickname, this.botCount(), this.level(), patch);
    } finally {
      this.busy.set(false);
    }
  }

  protected back(): void {
    void this.router.navigate(['/']);
  }
}
