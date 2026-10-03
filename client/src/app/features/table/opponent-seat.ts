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
 * contenu), et à côté une pastille avec son nom, son nombre de cartes et son état (SPEC §12.3).
 */
@Component({
  selector: 'app-opponent-seat',
  imports: [Avatar, CardBack],
  template: `
    <article class="seat" [class.current]="isCurrent()" [class.offline]="!seat().isConnected">
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
        <app-avatar [playerId]="seat().playerId" [nickname]="seat().nickname" />
        <div class="info">
          <p class="name">{{ seat().nickname }}</p>
          <p class="meta">
            <span
              >{{ seat().cardCount }} {{ seat().cardCount > 1 ? 'cartes' : 'carte'
              }}{{ seat().isConnected ? '' : ' · hors ligne' }}</span
            >
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

  protected readonly backs = computed(() =>
    Array.from({ length: Math.min(this.seat().cardCount, MAX_FAN_BACKS) }, (_, index) => index),
  );
  protected readonly extra = computed(() => Math.max(0, this.seat().cardCount - MAX_FAN_BACKS));
  protected readonly step = computed(() =>
    Math.min(MAX_STEP_DEGREES, MAX_FAN_DEGREES / Math.max(1, this.backs().length - 1)),
  );
}
