import { TestBed } from '@angular/core/testing';
import type { Card, PenaltyResponseOptions, SeatView } from '../../protocol/generated/protocol';
import { ChallengeDialog } from './challenge-dialog';
import { Hand } from './hand';
import { OpponentSeat } from './opponent-seat';

const seat = (cardCount: number): SeatView => ({
  playerId: 'p2',
  nickname: 'Max',
  seat: 1,
  cardCount,
  score: 0,
  isConnected: true,
  isBot: false,
  isHost: false,
  hasCalledUno: false,
  isReadyForNextRound: false,
});

function renderSeat(cardCount: number, forgotUno = false) {
  const fixture = TestBed.createComponent(OpponentSeat);
  fixture.componentRef.setInput('seat', seat(cardCount));
  fixture.componentRef.setInput('isCurrent', false);
  fixture.componentRef.setInput('forgotUno', forgotUno);
  fixture.detectChanges();
  return fixture.nativeElement as HTMLElement;
}

describe('OpponentSeat', () => {
  it('draws one card back per card held, from the card count alone', () => {
    expect(renderSeat(5).querySelectorAll('app-card-back')).toHaveLength(5);
  });

  it('caps the hand at 15 backs and leaves the exact count to the badge', () => {
    const host = renderSeat(20);

    expect(host.querySelectorAll('app-card-back')).toHaveLength(15);
    expect(host.querySelector('.count')?.textContent?.trim()).toBe('20');
  });

  it('flags a forgotten UNO on the seat', () => {
    expect(renderSeat(1, true).textContent).toContain('UNO oublié !');
    expect(renderSeat(1, false).textContent).not.toContain('UNO oublié !');
  });
});

describe('Hand', () => {
  it('packs the cards tighter as the hand grows, the last one keeping its full width', () => {
    const cards = (count: number): Card[] =>
      Array.from({ length: count }, (_, id) => ({ id, color: 'red', rank: '5' }));
    const columnsOf = (count: number): string => {
      const fixture = TestBed.createComponent(Hand);
      fixture.componentRef.setInput('cards', cards(count));
      fixture.componentRef.setInput('playableIds', []);
      fixture.componentRef.setInput('myTurn', true);
      fixture.detectChanges();
      return (
        (fixture.nativeElement as HTMLElement).querySelector<HTMLElement>('.hand')?.style
          .gridTemplateColumns ?? ''
      );
    };

    expect(columnsOf(1)).toBe('var(--card-w)');
    expect(columnsOf(8)).toContain('repeat(7,');
    expect(columnsOf(8)).toMatch(/var\(--card-w\)$/);
  });
});

describe('ChallengeDialog', () => {
  const plusFour: Card = { id: 9, color: null, rank: 'wildDrawFour' };

  function render(options: PenaltyResponseOptions) {
    HTMLDialogElement.prototype.showModal = function showModal(): void {
      this.setAttribute('open', '');
    };
    const fixture = TestBed.createComponent(ChallengeDialog);
    fixture.componentRef.setInput('card', plusFour);
    fixture.componentRef.setInput('color', 'blue');
    fixture.componentRef.setInput('options', options);
    const answers: string[] = [];
    fixture.componentInstance.accepted.subscribe(() => answers.push('accept'));
    fixture.componentInstance.challenged.subscribe(() => answers.push('challenge'));
    fixture.detectChanges();
    const buttons = Array.from(
      (fixture.nativeElement as HTMLElement).querySelectorAll<HTMLButtonElement>('button'),
    );
    return { host: fixture.nativeElement as HTMLElement, buttons, answers };
  }

  it('shows the +4, the rule in one line, and both answers', () => {
    const { host, buttons, answers } = render({ amount: 4, canChallenge: true, canStack: false });

    expect(host.querySelector('app-card')).not.toBeNull();
    expect(host.querySelector('.rule')?.textContent).toContain('bluff');
    expect(buttons.map((b) => b.textContent?.trim())).toEqual(['Contester', 'Accepter, piocher 4']);

    buttons[0].click();
    buttons[1].click();

    expect(answers).toEqual(['challenge', 'accept']);
  });

  it('only offers to accept when the card cannot be challenged', () => {
    const { buttons } = render({ amount: 4, canChallenge: false, canStack: false });

    expect(buttons.map((b) => b.textContent?.trim())).toEqual(['Accepter, piocher 4']);
  });
});
