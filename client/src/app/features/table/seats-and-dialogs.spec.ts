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

function renderSeat(cardCount: number, forgotUno = false, isBot = false) {
  const fixture = TestBed.createComponent(OpponentSeat);
  fixture.componentRef.setInput('seat', { ...seat(cardCount), isBot });
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

  it('wears a Bot badge instead of the connection state when the player is a bot', () => {
    const bot = renderSeat(5, false, true);
    const human = renderSeat(5);

    expect(bot.querySelector('.bot-badge')?.textContent?.trim()).toBe('Bot');
    expect(bot.textContent).not.toContain('en ligne');
    expect(human.querySelector('.bot-badge')).toBeNull();
    expect(human.textContent).toContain('en ligne');
  });

  it('flags a forgotten UNO on the seat', () => {
    expect(renderSeat(1, true).textContent).toContain('UNO oublié !');
    expect(renderSeat(1, false).textContent).not.toContain('UNO oublié !');
  });
});

describe('Hand', () => {
  const cards = (count: number): Card[] =>
    Array.from({ length: count }, (_, id) => ({ id, color: 'red', rank: '5' }));

  function render(shown: Card[]) {
    const fixture = TestBed.createComponent(Hand);
    fixture.componentRef.setInput('cards', shown);
    fixture.componentRef.setInput('playableIds', []);
    fixture.componentRef.setInput('myTurn', true);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    const slots = (): HTMLElement[] => Array.from(host.querySelectorAll<HTMLElement>('li'));
    return { fixture, host, slots };
  }

  it('spreads the cards around the centre of the fan, one offset per card', () => {
    const { host, slots } = render(cards(4));

    expect(slots().map((slot) => slot.style.getPropertyValue('--d'))).toEqual([
      '-1.5',
      '-0.5',
      '0.5',
      '1.5',
    ]);
    expect(host.querySelector<HTMLElement>('.hand')?.style.getPropertyValue('--n')).toBe('4');
  });

  it('lays out only the cards that have arrived, and re-spreads the fan when one more arrives', () => {
    const { fixture, host, slots } = render(cards(2));
    fixture.componentRef.setInput('incoming', [2, 3]);
    fixture.detectChanges();

    expect(slots()).toHaveLength(2);
    expect(host.querySelector('[data-anchor="card:2"]')).toBeNull();

    fixture.componentRef.setInput('cards', cards(3));
    fixture.componentRef.setInput('incoming', [3]);
    fixture.detectChanges();

    expect(slots().map((slot) => slot.style.getPropertyValue('--d'))).toEqual(['-1', '0', '1']);
    expect(host.querySelector<HTMLElement>('.hand')?.style.getPropertyValue('--n')).toBe('3');
  });

  it('knows no place for a card that is neither in the hand nor on its way', () => {
    const { fixture } = render(cards(2));

    expect(fixture.componentInstance.lastRect(42)).toBeNull();
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
