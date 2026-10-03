import { Component, computed, effect, input, output, signal } from '@angular/core';
import type { Color, PlayerView } from '../../protocol/generated/protocol';
import { ChallengeDialog } from './challenge-dialog';
import { ColorPicker } from './color-picker';
import { Hand } from './hand';
import { MatchOverDialog } from './match-over-dialog';
import { OpponentSeat } from './opponent-seat';
import { Piles } from './piles';
import { RoundOverDialog } from './round-over-dialog';
import { opponentsInViewOrder, seatSlots } from './seat-layout';

const CLOCK_TICK_MS = 250;
const JOURNAL_LINES = 2;

export interface CardPlay {
  readonly cardId: number;
  readonly color?: Color;
}

/** Un bouton « Contre-UNO ! » : un par joueur dont la fenêtre est ouverte. */
interface CatchButton {
  readonly playerId: string;
  readonly nickname: string;
  /** Pendant la grâce, seul le fautif peut encore annoncer : le bouton est grisé avec son compte à rebours. */
  readonly inGrace: boolean;
  readonly secondsLeft: number;
}

/**
 * La table, sans état serveur : tout arrive par des entrées et repart par des sorties (le conteneur `Table` y branche
 * le store, la page `/dev/table` des scénarios écrits à la main). Moi en bas, les adversaires en arc en haut.
 */
@Component({
  selector: 'app-table-view',
  imports: [
    OpponentSeat,
    Piles,
    Hand,
    ChallengeDialog,
    ColorPicker,
    RoundOverDialog,
    MatchOverDialog,
  ],
  templateUrl: './table-view.html',
  styleUrl: './table-view.scss',
})
export class TableView {
  readonly view = input.required<PlayerView>();
  /** Les dernières lignes du journal de la partie, la plus récente en dernier. */
  readonly journal = input<readonly string[]>([]);
  /** Écart entre l'horloge du serveur et celle du navigateur, pour les comptes à rebours. */
  readonly clockOffset = input(0);
  /** Résultat d'une contestation, affiché en bandeau un instant. */
  readonly banner = input<string | null>(null);

  readonly cardPlayed = output<CardPlay>();
  readonly colorChosen = output<Color>();
  readonly deckClicked = output<void>();
  readonly unoCalled = output<void>();
  readonly caught = output<string>();
  readonly penaltyAnswered = output<'accept' | 'challenge'>();
  readonly nextRoundAsked = output<void>();
  readonly left = output<void>();

  /** Joker choisi dans la main, en attente de sa couleur. */
  protected readonly pendingWild = signal<number | null>(null);
  private readonly now = signal(Date.now());
  private readonly serverNow = computed(() => this.now() + this.clockOffset());

  protected readonly mySeat = computed(() =>
    this.view().players.find((p) => p.playerId === this.view().me.playerId),
  );
  protected readonly myTurn = computed(
    () => this.view().currentPlayerId === this.view().me.playerId,
  );
  protected readonly opponents = computed(() =>
    opponentsInViewOrder(this.view().players, this.mySeat()?.seat ?? 0),
  );
  protected readonly slots = computed(() => seatSlots(this.opponents().length));
  protected readonly currentName = computed(() => this.nameOf(this.view().currentPlayerId));
  protected readonly turnSeconds = computed(() => this.secondsUntil(this.view().turnDeadline));
  protected readonly nextRoundSeconds = computed(() =>
    this.secondsUntil(this.view().nextRoundDeadline),
  );
  protected readonly lastLines = computed(() => this.journal().slice(-JOURNAL_LINES));

  /** Les fenêtres de contre-UNO ouvertes sur d'autres joueurs que moi, avec ce qu'il reste de leur temps. */
  private readonly otherWindows = computed(() =>
    this.view().unoWindows.filter(
      (window) =>
        window.targetId !== this.view().me.playerId && this.serverNow() < window.expiresAt,
    ),
  );
  protected readonly forgotten = computed(
    () => new Set(this.otherWindows().map((window) => window.targetId)),
  );
  protected readonly catchButtons = computed<readonly CatchButton[]>(() =>
    this.otherWindows().map((window) => {
      const inGrace = this.serverNow() < window.graceEndsAt;
      const end = inGrace ? window.graceEndsAt : window.expiresAt;
      return {
        playerId: window.targetId,
        nickname: this.nameOf(window.targetId),
        inGrace,
        secondsLeft: Math.max(0, Math.ceil((end - this.serverNow()) / 1000)),
      };
    }),
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
      this.cardPlayed.emit({ cardId });
    }
  }

  protected playWild(color: Color): void {
    const cardId = this.pendingWild();
    this.pendingWild.set(null);
    if (cardId !== null) {
      this.cardPlayed.emit({ cardId, color });
    }
  }

  /** Un bouton grisé (pendant la grâce) est atténué mais reste focalisable ; le clic n'y fait rien. */
  protected tryCatch(button: CatchButton): void {
    if (!button.inGrace) {
      this.caught.emit(button.playerId);
    }
  }

  private nameOf(playerId: string): string {
    return this.view().players.find((p) => p.playerId === playerId)?.nickname ?? '?';
  }

  /** Secondes restantes avant une échéance donnée en heure du serveur. */
  private secondsUntil(deadline: number | null): number | null {
    return deadline === null ? null : Math.max(0, Math.ceil((deadline - this.serverNow()) / 1000));
  }
}
