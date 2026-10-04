import { Component, computed, inject, input, signal } from '@angular/core';
import { Router } from '@angular/router';
import type {
  DrawAmount,
  DrawRule,
  MatchLength,
  RoomView,
  TurnTimerSeconds,
} from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { Avatar } from '../../ui/avatar';
import { Button } from '../../ui/button';
import { Icon } from '../../ui/icon';
import { Logo } from '../../ui/logo';
import { MotionSetting } from '../../ui/motion-setting';
import { SegmentOption, Segmented } from '../../ui/segmented';

const MATCH_LENGTHS: readonly SegmentOption<MatchLength>[] = [
  { value: 'singleRound', label: 'Manche unique' },
  { value: 'to250', label: '250 points' },
  { value: 'to500', label: '500 points' },
];
const MATCH_LENGTH_SUMMARY: Record<MatchLength, string> = {
  singleRound: 'Manche unique',
  to250: 'Partie en 250 points',
  to500: 'Partie en 500 points',
};

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

const DRAW_AMOUNTS: readonly SegmentOption<DrawAmount>[] = [
  { value: 'untilPlayable', label: 'Jusqu’à pouvoir jouer' },
  { value: 'one', label: '1 carte' },
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

const COPIED_FEEDBACK_MS = 2000;

/** Salon avant la partie : joueurs à gauche, réglages à droite, un grand bouton en bas (SPEC §12.2). */
@Component({
  selector: 'app-lobby',
  imports: [Avatar, Button, Icon, Logo, MotionSetting, Segmented],
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
  protected readonly drawAmounts = DRAW_AMOUNTS;
  protected readonly lastCardRules = LAST_CARD_RULES;

  /** Joueur dont l'exclusion attend une confirmation. */
  protected readonly kickCandidate = signal<string | null>(null);
  protected readonly copied = signal(false);
  protected readonly hintShown = signal(false);

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
      } · ${s.declareUnoToWin ? 'UNO obligatoire pour gagner' : 'dernière carte libre'}`,
    };
  });

  protected lastCardRule(): LastCardRule {
    return this.room().settings.declareUnoToWin ? 'declare' : 'free';
  }

  protected setLastCardRule(rule: LastCardRule): void {
    void this.store.updateSettings({ declareUnoToWin: rule === 'declare' });
  }

  protected async copyCode(): Promise<void> {
    try {
      await navigator.clipboard.writeText(`${location.origin}/r/${this.room().code}`);
      this.copied.set(true);
      setTimeout(() => this.copied.set(false), COPIED_FEEDBACK_MS);
    } catch {
      this.store.notify('Impossible de copier : fais-le à la main.');
    }
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
