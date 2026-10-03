import { TestBed } from '@angular/core/testing';
import type { Card } from '../../protocol/generated/protocol';
import { Hand } from './hand';

const cards: Card[] = [
  { id: 1, color: 'red', rank: '5' },
  { id: 2, color: 'blue', rank: '7' },
];

function render(myTurn: boolean) {
  const fixture = TestBed.createComponent(Hand);
  fixture.componentRef.setInput('cards', cards);
  fixture.componentRef.setInput('playableIds', [1]);
  fixture.componentRef.setInput('myTurn', myTurn);
  fixture.detectChanges();
  const buttons = Array.from(
    (fixture.nativeElement as HTMLElement).querySelectorAll<HTMLButtonElement>('button.slot'),
  );
  return { fixture, buttons };
}

describe('Hand', () => {
  it('only lets me click playable cards on my turn, and announces them', () => {
    const { fixture, buttons } = render(true);
    const played: number[] = [];
    fixture.componentInstance.played.subscribe((id) => played.push(id));

    buttons.forEach((button) => button.click());

    expect(buttons.map((b) => b.disabled)).toEqual([false, true]);
    expect(buttons[0].getAttribute('aria-label')).toBe('5 rouge, jouable');
    expect(played).toEqual([1]);
  });

  it('disables every card when it is not my turn', () => {
    const { buttons } = render(false);

    expect(buttons.every((b) => b.disabled)).toBe(true);
  });

  it('lowers the cards I cannot play on my turn, and keeps a neutral hand otherwise', () => {
    const mine = render(true).buttons.map((b) => b.classList.contains('unplayable'));
    const theirs = render(false).buttons.map((b) => b.classList.contains('unplayable'));

    expect(mine).toEqual([false, true]);
    expect(theirs).toEqual([false, false]);
  });
});
