import { Component, computed, input } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Avatar } from '../../ui/avatar';
import { CardBack } from '../../ui/card-back';

/** Au-delà de ce nombre de dos de cartes, l'éventail s'arrête et un « +N » prend le relais. */
export const MAX_FAN_BACKS = 12;
const MAX_FAN_DEGREES = 56;
const MAX_STEP_DEGREES = 7;

/**
 * Un adversaire : ses cartes de dos en éventail (dessinées d'après le seul nombre de cartes, jamais d'après leur
 * contenu), orienté vers le centre de la table (incliné sur les côtés), et une pastille avec l'avatar, le nombre de cartes, le pseudo et
 * l'état de connexion. Quand c'est son tour : lueur et anneau de minuteur autour de l'avatar (SPEC §12.3).
 */
@Component({
  selector: 'app-opponent-seat',
  imports: [Avatar, CardBack],
  template: `
    <article
      class="seat"
      [class.current]="isCurrent()"
      [class.compact]="compact()"
      [class.offline]="!seat().isConnected"
      [style.--tilt.deg]="rotation()"
    >
      <div
        class="fan"
        aria-hidden="true"
        [style.--step.deg]="step()"
        [style.--count]="backs().length"
      >
        @for (back of backs(); track back) {
          <app-card-back class="back" [style.--i]="back" />
        }
        @if (extra() > 0) {
          <span class="more">+{{ extra() }}</span>
        }
      </div>
      <div class="pill">
        <div
          class="portrait"
          [class.timed]="timerFraction() !== null"
          [style.--f]="timerFraction() ?? 0"
        >
          <app-avatar [playerId]="seat().playerId" [nickname]="seat().nickname" />
          <span class="count" [attr.aria-label]="seat().cardCount + ' cartes'">{{
            seat().cardCount
          }}</span>
        </div>
        <div class="ribbon">
          <p class="name">{{ seat().nickname }}</p>
          <p class="state">
            <span class="dot" [class.off]="!seat().isConnected" aria-hidden="true"></span>
            {{ seat().isConnected ? 'en ligne' : 'hors ligne' }}
          </p>
        </div>
        @if (forgotUno()) {
          <span class="forgot">UNO oublié !</span>
        }
        @if (isCurrent()) {
          <span class="sr-only">C’est son tour</span>
        }
      </div>
    </article>
  `,
  styleUrl: './opponent-seat.scss',
})
export class OpponentSeat {
  readonly seat = input.required<SeatView>();
  readonly isCurrent = input.required<boolean>();
  /** Sa fenêtre de contre-UNO est ouverte : il n'a qu'une carte et n'a pas annoncé UNO. */
  readonly forgotUno = input.required<boolean>();
  /** Inclinaison de l'éventail, en degrés (orienté vers le centre de la table sur les côtés). */
  readonly rotation = input(0);
  readonly compact = input(false);
  /** Nombre maximal de dos dessinés (moins sur un petit écran). */
  readonly maxBacks = input(MAX_FAN_BACKS);
  /** Part du temps de tour qu'il reste (0 à 1) si c'est son tour et qu'il y a un minuteur, sinon `null`. */
  readonly timerFraction = input<number | null>(null);

  protected readonly backs = computed(() =>
    Array.from({ length: Math.min(this.seat().cardCount, this.maxBacks()) }, (_, index) => index),
  );
  protected readonly extra = computed(() => Math.max(0, this.seat().cardCount - this.maxBacks()));
  protected readonly step = computed(() =>
    Math.min(MAX_STEP_DEGREES, MAX_FAN_DEGREES / Math.max(1, this.backs().length - 1)),
  );
}
