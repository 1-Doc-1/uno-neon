import { Component, input } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Avatar } from '../../ui/avatar';

/** Ma pastille, en bas à gauche : avatar, nombre de cartes, pseudo, score et minuteur quand c'est mon tour. */
@Component({
  selector: 'app-my-badge',
  imports: [Avatar],
  template: `
    <div class="me" [class.current]="myTurn()">
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
      <div class="info">
        <p class="name">{{ seat().nickname }}</p>
        <p class="meta">
          {{ seat().score }} pts
          @if (timerSeconds() !== null) {
            · <span class="timer">{{ timerSeconds() }} s</span>
          }
        </p>
      </div>
    </div>
  `,
  styleUrl: './my-badge.scss',
})
export class MyBadge {
  readonly seat = input.required<SeatView>();
  readonly myTurn = input.required<boolean>();
  /** Part du temps de tour qu'il reste (0 à 1), ou `null` sans minuteur ou hors de mon tour. */
  readonly timerFraction = input<number | null>(null);
  readonly timerSeconds = input<number | null>(null);
}
