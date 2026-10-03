import { Component, computed, inject, input, signal } from '@angular/core';
import { Router } from '@angular/router';
import type {
  DrawRule,
  MatchLength,
  RoomView,
  TurnTimerSeconds,
} from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { Avatar } from '../../ui/avatar';
import { NeonButton } from '../../ui/neon-button';
import { SegmentOption, Segmented } from '../../ui/segmented';

const MATCH_LENGTHS: readonly SegmentOption<MatchLength>[] = [
  { value: 'singleRound', label: 'Manche unique' },
  { value: 'to250', label: '250 points' },
  { value: 'to500', label: '500 points' },
];

const TURN_TIMERS: readonly SegmentOption<TurnTimerSeconds>[] = [
  { value: 0, label: 'Sans' },
  { value: 15, label: '15 s' },
  { value: 30, label: '30 s' },
  { value: 60, label: '60 s' },
];

const DRAW_RULES: readonly SegmentOption<DrawRule>[] = [
  { value: 'guided', label: 'Guidée' },
  { value: 'official', label: 'Officielle' },
];

type LastCardRule = 'free' | 'declare';

const LAST_CARD_RULES: readonly SegmentOption<LastCardRule>[] = [
  { value: 'free', label: 'Libre' },
  { value: 'declare', label: 'UNO obligatoire' },
];

const MAX_PLAYERS: readonly SegmentOption<number>[] = [2, 3, 4, 5, 6, 7, 8, 9, 10].map((n) => ({
  value: n,
  label: String(n),
}));

/** Salon avant la partie : code à partager, joueurs, réglages de l'hôte, prêt / lancer. */
@Component({
  selector: 'app-lobby',
  imports: [Avatar, NeonButton, Segmented],
  templateUrl: './lobby.html',
  styleUrl: './lobby.scss',
})
export class Lobby {
  protected readonly store = inject(GameStore);
  private readonly router = inject(Router);

  readonly room = input.required<RoomView>();

  protected readonly matchLengths = MATCH_LENGTHS;
  protected readonly turnTimers = TURN_TIMERS;
  protected readonly maxPlayers = MAX_PLAYERS;
  protected readonly drawRules = DRAW_RULES;
  protected readonly lastCardRules = LAST_CARD_RULES;

  protected lastCardRule(): LastCardRule {
    return this.room().settings.declareUnoToWin ? 'declare' : 'free';
  }

  protected setLastCardRule(rule: LastCardRule): void {
    void this.store.updateSettings({ declareUnoToWin: rule === 'declare' });
  }

  /** Joueur dont l'exclusion attend une confirmation. */
  protected readonly kickCandidate = signal<string | null>(null);

  protected readonly me = computed(() =>
    this.room().players.find((p) => p.playerId === this.store.playerId()),
  );
  protected readonly isHost = computed(() => this.me()?.isHost ?? false);

  /** Pourquoi l'hôte ne peut pas lancer la partie, ou `null` s'il le peut. */
  protected readonly startBlocker = computed(() => {
    const players = this.room().players;
    if (players.length < 2) {
      return 'Il faut au moins 2 joueurs pour commencer.';
    }
    const waiting = players.filter((p) => !p.isReady && !p.isHost).map((p) => p.nickname);
    return waiting.length > 0 ? `En attente de ${waiting.join(', ')}.` : null;
  });

  protected async copy(text: string, confirmation: string): Promise<void> {
    try {
      await navigator.clipboard.writeText(text);
      this.store.notify(confirmation);
    } catch {
      this.store.notify('Impossible de copier : fais-le à la main.');
    }
  }

  protected copyCode(): Promise<void> {
    return this.copy(this.room().code, 'Code copié');
  }

  protected copyLink(): Promise<void> {
    return this.copy(`${location.origin}/r/${this.room().code}`, 'Lien copié');
  }

  protected toggleReady(): void {
    void this.store.setReady(!this.me()?.isReady);
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
