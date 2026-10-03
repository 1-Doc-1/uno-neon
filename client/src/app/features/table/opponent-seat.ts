import { Component, input } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Avatar } from '../../ui/avatar';
import { MAX_FAN_BACKS, OpponentFan } from './opponent-fan';

/**
 * Un adversaire : son éventail de dos au-dessus d'une pastille avec l'avatar, le nombre de cartes, le pseudo et l'état
 * de connexion. Quand c'est son tour : lueur et anneau de minuteur autour de l'avatar (SPEC §12.3).
 */
@Component({
  selector: 'app-opponent-seat',
  imports: [Avatar, OpponentFan],
  template: `
    <article
      class="seat"
      [class.current]="isCurrent()"
      [class.compact]="compact()"
      [class.offline]="!seat().isConnected"
    >
      <app-opponent-fan class="fan" [count]="seat().cardCount" [maxBacks]="maxBacks()" />
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
  readonly compact = input(false);
  readonly maxBacks = input(MAX_FAN_BACKS);
  /** Part du temps de tour qu'il reste (0 à 1) si c'est son tour et qu'il y a un minuteur, sinon `null`. */
  readonly timerFraction = input<number | null>(null);
}
