import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it } from 'vitest';
import { GAME_TRANSPORT } from '../core/game-transport';
import { GameStore } from '../state/game-store';
import { FakeTransport } from '../testing/fake-transport';
import { SessionBusy } from './session-busy';

describe('SessionBusy', () => {
  let transport: FakeTransport;

  beforeEach(() => {
    HTMLDialogElement.prototype.showModal = function showModal(): void {
      this.setAttribute('open', '');
    };
    transport = new FakeTransport();
    TestBed.configureTestingModule({
      providers: [{ provide: GAME_TRANSPORT, useValue: transport }],
    });
  });

  it('says nothing while the session is ours', () => {
    TestBed.inject(GameStore);
    TestBed.tick();
    transport.receive({ v: 1, type: 'session.welcome', payload: { playerId: 'p1' } });
    const fixture = TestBed.createComponent(SessionBusy);
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).not.toContain('autre onglet');
  });

  it('says the game is open in another tab, and plays here on a click', () => {
    TestBed.inject(GameStore);
    TestBed.tick();
    transport.receive({ v: 1, type: 'session.busy', payload: {} });
    const fixture = TestBed.createComponent(SessionBusy);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;

    expect(host.textContent).toContain('Partie ouverte dans un autre onglet');
    host.querySelector<HTMLButtonElement>('button')?.click();

    expect(transport.sent.at(-1)).toMatchObject({ type: 'session.takeOver' });
  });

  it('also shows when this tab was closed because another one took the session', () => {
    TestBed.inject(GameStore);
    TestBed.tick();
    transport.receive({ v: 1, type: 'session.welcome', payload: { playerId: 'p1' } });
    transport.statusState.set('closed');
    transport.takenOverState.set(true);
    TestBed.inject(GameStore);
    TestBed.tick();
    const fixture = TestBed.createComponent(SessionBusy);
    fixture.detectChanges();

    expect(fixture.nativeElement.textContent).toContain('Jouer ici');
  });
});
