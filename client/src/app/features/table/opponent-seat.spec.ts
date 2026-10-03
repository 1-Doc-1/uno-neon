import { TestBed } from '@angular/core/testing';
import type { SeatView } from '../../protocol/generated/protocol';
import { OpponentSeat } from './opponent-seat';

const seat: SeatView = {
  playerId: 'p2',
  nickname: 'Max',
  seat: 1,
  cardCount: 1,
  score: 0,
  isConnected: true,
  isBot: false,
  isHost: false,
  hasCalledUno: false,
  isReadyForNextRound: false,
};

describe('OpponentSeat', () => {
  it('offers the counter-UNO whether or not it is that opponent’s turn', () => {
    for (const isCurrent of [true, false]) {
      const fixture = TestBed.createComponent(OpponentSeat);
      fixture.componentRef.setInput('seat', seat);
      fixture.componentRef.setInput('isCurrent', isCurrent);
      fixture.componentRef.setInput('catchable', true);
      fixture.detectChanges();
      let caught = 0;
      fixture.componentInstance.caught.subscribe(() => caught++);

      const button = (fixture.nativeElement as HTMLElement).querySelector<HTMLButtonElement>(
        '.catch',
      );
      button?.click();

      expect(button).not.toBeNull();
      expect(caught).toBe(1);
    }
  });
});
