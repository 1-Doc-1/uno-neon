import { Component, computed, inject, input, signal } from '@angular/core';
import { Router } from '@angular/router';
import type { BotLevel, MatchLength, RoomView } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { Avatar } from '../../ui/avatar';
import { Button } from '../../ui/button';
import { Icon } from '../../ui/icon';
import { Logo } from '../../ui/logo';
import { SegmentOption, Segmented } from '../../ui/segmented';
import { BOT_LEVELS } from './bot-levels';
import { deckSize } from './deck-size';
import { GameSettings } from './game-settings';
import { ClickSound } from '../../ui/click-sound';

const MATCH_LENGTH_SUMMARY: Record<MatchLength, string> = {
  singleRound: 'Manche unique',
  to250: 'Partie en 250 points',
  to500: 'Partie en 500 points',
};

const COPIED_FEEDBACK_MS = 2000;

/** Salon avant la partie : joueurs à gauche, réglages à droite, un grand bouton en bas (SPEC §12.2). */
@Component({
  selector: 'app-lobby',
  imports: [ClickSound, Avatar, Button, GameSettings, Icon, Logo, Segmented],
  templateUrl: './lobby.html',
  styleUrl: './lobby.scss',
})
export class Lobby {
  protected readonly store = inject(GameStore);
  private readonly router = inject(Router);

  readonly room = input.required<RoomView>();

  /** Joueur dont l'exclusion attend une confirmation. */
  protected readonly kickCandidate = signal<string | null>(null);
  protected readonly copied = signal(false);
  protected readonly hintShown = signal(false);
  protected readonly botLevel = signal<BotLevel>('normal');
  protected readonly botLevels: readonly SegmentOption<BotLevel>[] = BOT_LEVELS;
  protected readonly canAddBot = computed(
    () => this.room().players.length < this.room().settings.maxPlayers,
  );

  protected readonly me = computed(() =>
    this.room().players.find((p) => p.playerId === this.store.playerId()),
  );
  protected readonly isHost = computed(() => this.me()?.isHost ?? false);

  /** Pourquoi l'hôte ne peut pas lancer la partie, ou `null` s'il le peut. */
  protected readonly startBlocker = computed(() => {
    const players = this.room().players;
    if (players.length < 2) {
      return 'Il faut au moins 2 joueurs';
    }
    const waiting = players.filter((p) => !p.isReady && !p.isHost).map((p) => p.nickname);
    return waiting.length > 0 ? `En attente de : ${waiting.join(', ')}` : null;
  });

  /** Résumé en lecture seule des réglages, pour ceux qui ne sont pas l'hôte. */
  protected readonly summary = computed(() => {
    const s = this.room().settings;
    const timer = s.turnTimerSeconds === 0 ? 'sans minuteur' : `minuteur ${s.turnTimerSeconds} s`;
    return {
      first: `${MATCH_LENGTH_SUMMARY[s.matchLength]} · ${timer} · ${s.maxPlayers} joueurs max`,
      second: `Pioche ${s.drawRule === 'guided' ? 'guidée' : 'officielle'}, ${
        s.drawAmount === 'untilPlayable' ? 'jusqu’à pouvoir jouer' : '1 carte'
      } · ${s.declareUnoToWin ? 'UNO obligatoire pour gagner' : 'dernière carte libre'} · ${
        s.stacking === 'ladder' ? 'pénalités cumulées (+2 < +4 < +5)' : 'pénalités sans cumul'
      }`,
      third: `Paquet de ${deckSize(s)} cartes : +2 ×${s.drawTwoMultiplier}, +4 ×${s.wildDrawFourMultiplier}, +5 ×${s.wildDrawFiveMultiplier}`,
    };
  });

  protected async copyCode(): Promise<void> {
    try {
      await navigator.clipboard.writeText(`${location.origin}/r/${this.room().code}`);
      this.copied.set(true);
      setTimeout(() => this.copied.set(false), COPIED_FEEDBACK_MS);
    } catch {
      this.store.notify('Impossible de copier : fais-le à la main.');
    }
  }

  protected addBot(): void {
    void this.store.addBot(this.botLevel());
  }

  protected toggleReady(): void {
    void this.store.setReady(!this.me()?.isReady);
  }

  /** « Lancer » atténué (aria-disabled) reste cliquable : le clic explique pourquoi au lieu de ne rien faire. */
  protected start(): void {
    if (this.startBlocker() !== null) {
      this.hintShown.set(true);
      return;
    }
    void this.store.startMatch();
  }

  protected askKick(playerId: string): void {
    this.kickCandidate.set(playerId);
  }

  protected async confirmKick(playerId: string): Promise<void> {
    this.kickCandidate.set(null);
    await this.store.kick(playerId);
  }

  protected async leave(): Promise<void> {
    if (await this.store.leaveRoom()) {
      await this.router.navigate(['/']);
    }
  }
}
