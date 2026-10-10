import { Component, input, output } from '@angular/core';
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
      [class.facing]="facing()"
      [class.offline]="!seat().isConnected"
      [class.targetable]="targetable()"
      [attr.role]="targetable() ? 'button' : null"
      [attr.tabindex]="targetable() ? 0 : null"
      [attr.aria-label]="targetable() ? 'Viser ' + seat().nickname : null"
      (click)="onTarget()"
      (keydown.enter)="onTarget()"
      (keydown.space)="onTarget(); $event.preventDefault()"
    >
      <app-opponent-fan
        class="fan"
        [count]="seat().cardCount"
        [maxBacks]="maxBacks()"
        [yaw]="yaw()"
      />
      <div class="pill" [attr.data-anchor]="'seat:' + seat().playerId">
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
            @if (seat().isBot) {
              <span class="bot-badge">Bot</span>
            } @else {
              <span class="dot" [class.off]="!seat().isConnected" aria-hidden="true"></span>
              {{ seat().isConnected ? 'en ligne' : 'hors ligne' }}
            }
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
  /** Siège en face de moi : une grande main centrée. */
  readonly facing = input(false);
  /** Rotation de la main autour de l'axe vertical, vers le centre de la table (degrés). */
  readonly yaw = input(0);
  readonly maxBacks = input(MAX_FAN_BACKS);
  /** Part du temps de tour qu'il reste (0 à 1) si c'est son tour et qu'il y a un minuteur, sinon `null`. */
  readonly timerFraction = input<number | null>(null);
  /** Un Joker +5 cherche sa cible : ce siège se vise d'un clic (ou d'Entrée au clavier). */
  readonly targetable = input(false);
  readonly targeted = output<string>();

  protected onTarget(): void {
    if (this.targetable()) {
      this.targeted.emit(this.seat().playerId);
    }
  }
}
