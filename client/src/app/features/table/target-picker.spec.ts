import { TestBed } from '@angular/core/testing';
import type { SeatView } from '../../protocol/generated/protocol';
import { PlusFivePrompt } from './plus-five-prompt';
import { TargetPicker } from './target-picker';

const seat = (playerId: string, nickname: string, cardCount: number): SeatView => ({
  playerId,
  nickname,
  seat: 1,
  cardCount,
  score: 0,
  isConnected: true,
  isBot: false,
  isHost: false,
  hasCalledUno: false,
  isReadyForNextRound: false,
});

describe('TargetPicker', () => {
  function render() {
    HTMLDialogElement.prototype.show = function show(): void {
      this.setAttribute('open', '');
    };
    const fixture = TestBed.createComponent(TargetPicker);
    fixture.componentRef.setInput('targets', [seat('max', 'Max', 1), seat('lea', 'Léa', 12)]);
    const events: string[] = [];
    fixture.componentInstance.picked.subscribe((id) => events.push(`picked:${id}`));
    fixture.componentInstance.cancelled.subscribe(() => events.push('cancelled'));
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    return {
      host,
      events,
      targets: () => Array.from(host.querySelectorAll<HTMLElement>('.target')),
    };
  }

  it('lists the opponents with their initial, nickname and number of cards', () => {
    const { targets } = render();

    expect(targets()).toHaveLength(2);
    expect(targets()[0].textContent).toContain('Max');
    expect(targets()[0].textContent).toContain('1 carte');
    expect(targets()[0].textContent).not.toContain('1 cartes');
    expect(targets()[1].textContent).toContain('Léa');
    expect(targets()[1].textContent).toContain('12 cartes');
    expect(targets()[0].querySelector('app-avatar')).not.toBeNull();
  });

  it('is not modal, so that a seat of the table can be clicked instead', () => {
    const { host } = render();

    expect(host.querySelector('dialog')?.hasAttribute('open')).toBe(true);
    expect(host.querySelector('dialog')?.getAttribute('aria-label')).toBe(
      'Choisir la cible du Joker +5',
    );
  });

  it('picks a target by click', () => {
    const { targets, events } = render();

    targets()[1].click();

    expect(events).toEqual(['picked:lea']);
  });

  it('picks a target with the number keys and cancels with Escape, no mouse needed', () => {
    const { events } = render();

    document.dispatchEvent(new KeyboardEvent('keydown', { key: '1' }));
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '2' }));
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '7' })); // nobody sits there
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));

    expect(events).toEqual(['picked:max', 'picked:lea', 'cancelled']);
  });
});

describe('PlusFivePrompt', () => {
  function render(canStack: boolean) {
    const fixture = TestBed.createComponent(PlusFivePrompt);
    fixture.componentRef.setInput('options', { amount: 10, canChallenge: false, canStack });
    let accepted = 0;
    fixture.componentInstance.accepted.subscribe(() => accepted++);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    return { host, accepted: () => accepted };
  }

  it('says what is owed and lets the target accept it', () => {
    const { host, accepted } = render(false);

    expect(host.textContent).toContain('Tu dois piocher 10 cartes');
    host.querySelector<HTMLButtonElement>('button')?.click();

    expect(accepted()).toBe(1);
  });

  it('invites to answer with the Wild Draw Five held, only when there is one', () => {
    expect(render(true).host.textContent).toContain('réponds avec ton Joker +5');
    expect(render(false).host.textContent).not.toContain('réponds');
  });
});
