import { Component, input, output } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Avatar } from '../../ui/avatar';
import { CardBack } from '../../ui/card-back';

/** Un adversaire : avatar, pseudo, dos de cartes, état (tour, UNO, déconnexion) et contre-UNO. */
@Component({
  selector: 'app-opponent-seat',
  imports: [Avatar, CardBack],
  template: `
    <article class="seat glass" [class.current]="isCurrent()" [class.offline]="!seat().isConnected">
      <app-avatar [playerId]="seat().playerId" [nickname]="seat().nickname" />
      <div class="info">
        <p class="name">{{ seat().nickname }}</p>
        <p class="meta">
          <span class="count" [attr.aria-label]="seat().cardCount + ' cartes'">
            <app-card-back class="mini" />
            {{ seat().cardCount }}
          </span>
          <span class="score">{{ seat().score }} pts</span>
        </p>
        <p class="flags">
          @if (seat().cardCount === 1 && seat().hasCalledUno) {
            <span class="uno">UNO !</span>
          }
          @if (!seat().isConnected) {
            <span class="off">Déconnecté</span>
          }
          @if (isCurrent()) {
            <span class="turn">Son tour</span>
          }
        </p>
      </div>
      @if (catchable()) {
        <button type="button" class="catch" (click)="caught.emit()">Contre-UNO !</button>
      }
    </article>
  `,
  styleUrl: './opponent-seat.scss',
})
export class OpponentSeat {
  readonly seat = input.required<SeatView>();
  readonly isCurrent = input.required<boolean>();
  readonly catchable = input.required<boolean>();
  readonly caught = output<void>();
}
