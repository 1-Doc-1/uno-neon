import { Component, computed, effect, inject, input, signal } from '@angular/core';
import { Router } from '@angular/router';
import type { Color, PlayerView } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { NeonButton } from '../../ui/neon-button';
import { ColorPicker } from './color-picker';
import { describeEvent } from './describe-event';
import { Hand } from './hand';
import { MatchOverDialog } from './match-over-dialog';
import { OpponentSeat } from './opponent-seat';
import { PenaltyBar } from './penalty-bar';
import { Piles } from './piles';
import { RoundOverDialog } from './round-over-dialog';

const CLOCK_TICK_MS = 500;
const LOG_LINES = 4;

/** Table de jeu : adversaires, piles, ma main et les actions ; le serveur décide, ici on affiche et on relaie. */
@Component({
  selector: 'app-table',
  imports: [
    NeonButton,
    OpponentSeat,
    Piles,
    Hand,
    PenaltyBar,
    ColorPicker,
    RoundOverDialog,
    MatchOverDialog,
  ],
  templateUrl: './table.html',
  styleUrl: './table.scss',
})
export class Table {
  private readonly store = inject(GameStore);
  private readonly router = inject(Router);

  readonly view = input.required<PlayerView>();

  /** Joker choisi dans la main, en attente de sa couleur. */
  protected readonly pendingWild = signal<number | null>(null);
  private readonly now = signal(Date.now());

  protected readonly opponents = computed(() =>
    this.view().players.filter((p) => p.playerId !== this.view().me.playerId),
  );
  protected readonly myTurn = computed(() => this.store.isMyTurn());
  protected readonly mySeat = computed(() =>
    this.view().players.find((p) => p.playerId === this.view().me.playerId),
  );
  protected readonly currentName = computed(() => this.nameOf(this.view().currentPlayerId));
  protected readonly turnSeconds = computed(() => this.secondsUntil(this.view().turnDeadline));
  protected readonly nextRoundSeconds = computed(() =>
    this.secondsUntil(this.view().nextRoundDeadline),
  );
  protected readonly journal = computed(() =>
    this.store
      .recentEvents()
      .map((event) => describeEvent(event, (id) => this.nameOf(id)))
      .filter((line): line is string => line !== null)
      .slice(-LOG_LINES),
  );

  constructor() {
    effect((onCleanup) => {
      const timer = setInterval(() => this.now.set(Date.now()), CLOCK_TICK_MS);
      onCleanup(() => clearInterval(timer));
    });
  }

  protected play(cardId: number): void {
    const card = this.view().me.hand.find((c) => c.id === cardId);
    if (card?.color === null) {
      this.pendingWild.set(cardId);
    } else {
      void this.store.playCard(cardId);
    }
  }

  protected playWild(color: Color): void {
    const cardId = this.pendingWild();
    this.pendingWild.set(null);
    if (cardId !== null) {
      void this.store.playCard(cardId, color);
    }
  }

  protected chooseColor(color: Color): void {
    void this.store.chooseColor(color);
  }

  protected draw(): void {
    void this.store.draw();
  }

  protected pass(): void {
    void this.store.pass();
  }

  /** Le serveur décide ; on n'affiche le bouton qu'une fois la grâce de 2 s écoulée. */
  protected canCatch(playerId: string): boolean {
    const serverNow = this.now() + this.store.serverClockOffset();
    return this.view().unoWindows.some(
      (window) => window.targetId === playerId && serverNow >= window.graceEndsAt,
    );
  }

  protected callUno(): void {
    void this.store.callUno();
  }

  protected catchUno(targetId: string): void {
    void this.store.catchUno(targetId);
  }

  protected respond(response: 'accept' | 'challenge'): void {
    void this.store.respondPenalty(response);
  }

  protected nextRound(): void {
    void this.store.readyForNextRound();
  }

  protected async leave(): Promise<void> {
    if (await this.store.leaveRoom()) {
      await this.router.navigate(['/']);
    }
  }

  private nameOf(playerId: string): string {
    return this.view().players.find((p) => p.playerId === playerId)?.nickname ?? '?';
  }

  /** Secondes restantes avant une échéance donnée en heure du serveur. */
  private secondsUntil(deadline: number | null): number | null {
    if (deadline === null) {
      return null;
    }
    const serverNow = this.now() + this.store.serverClockOffset();
    return Math.max(0, Math.ceil((deadline - serverNow) / 1000));
  }
}
