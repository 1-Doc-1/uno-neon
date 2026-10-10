import { Component, computed, effect, inject, input, signal } from '@angular/core';
import { Router } from '@angular/router';
import type { ClientEvent, Color, PlayerView } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { describeEvent } from './describe-event';
import { playedCards } from './fx/discard-pile';
import { CardPlay, TableView } from './table-view';

const JOURNAL_LINES = 4;
const BANNER_MS = 5000;

/** Conteneur de la table : relie la vue du serveur au store et traduit les gestes du joueur en intentions. */
@Component({
  selector: 'app-table',
  imports: [TableView],
  template: `
    <app-table-view
      [view]="view()"
      [journal]="journal()"
      [clockOffset]="store.serverClockOffset()"
      [banner]="banner()"
      [batch]="store.eventBatch()"
      [played]="played()"
      (cardPlayed)="play($event)"
      (colorChosen)="chooseColor($event)"
      (deckClicked)="onDeck()"
      (unoCalled)="store.callUno()"
      (caught)="store.catchUno($event)"
      (penaltyAnswered)="store.respondPenalty($event)"
      (nextRoundAsked)="store.readyForNextRound()"
      (left)="leave()"
    />
  `,
})
export class Table {
  protected readonly store = inject(GameStore);
  private readonly router = inject(Router);

  readonly view = input.required<PlayerView>();

  protected readonly banner = signal<string | null>(null);
  private lastChallenge: ClientEvent | null = null;

  protected readonly journal = computed(() =>
    this.store
      .recentEvents()
      .map((event) => describeEvent(event, (id) => this.nameOf(id)))
      .filter((line): line is string => line !== null)
      .slice(-JOURNAL_LINES),
  );

  protected readonly played = computed(() => playedCards(this.store.recentEvents()));

  constructor() {
    // Le verdict d'une contestation s'affiche un instant en bandeau
    effect(() => {
      const challenge = [...this.store.recentEvents()]
        .reverse()
        .find((event) => event.kind === 'challengeResolved');
      if (challenge && challenge !== this.lastChallenge) {
        this.lastChallenge = challenge;
        this.banner.set(describeEvent(challenge, (id) => this.nameOf(id)));
        setTimeout(() => this.banner.set(null), BANNER_MS);
      }
    });
  }

  protected play(play: CardPlay): void {
    void this.store.playCard(play.cardId, play.color, play.targetId);
  }

  protected chooseColor(color: Color): void {
    void this.store.chooseColor(color);
  }

  /** Le paquet sert à piocher ou à garder la carte piochée : c'est le serveur qui dit lequel des deux est permis. */
  protected onDeck(): void {
    const me = this.view().me;
    if (me.canKeepDrawnCard) {
      void this.store.pass();
    } else if (me.canDraw) {
      void this.store.draw();
    }
  }

  protected async leave(): Promise<void> {
    if (await this.store.leaveRoom()) {
      await this.router.navigate(['/']);
    }
  }

  private nameOf(playerId: string): string {
    return this.view().players.find((p) => p.playerId === playerId)?.nickname ?? '?';
  }
}
