import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { GAME_TRANSPORT } from '../../core/game-transport';
import type { RoomMember, RoomView } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { FakeTransport } from '../../testing/fake-transport';
import { Lobby } from './lobby';

const member = (
  playerId: string,
  nickname: string,
  extra: Partial<RoomMember> = {},
): RoomMember => ({
  playerId,
  nickname,
  seat: 0,
  isHost: false,
  isReady: false,
  isConnected: true,
  isBot: false,
  ...extra,
});

function roomOf(players: RoomMember[]): RoomView {
  return {
    code: 'K7M4XP',
    phase: 'lobby',
    settings: {
      stacking: 'off',
      jumpIn: false,
      sevenZero: false,
      drawAmount: 'untilPlayable',
      wildDrawFourMode: 'officialChallenge',
      turnTimerSeconds: 30,
      matchLength: 'to500',
      maxPlayers: 6,
      drawRule: 'guided',
      declareUnoToWin: false,
    },
    players,
  };
}

describe('Lobby', () => {
  let transport: FakeTransport;

  function render(me: string, players: RoomMember[]) {
    sessionStorage.clear();
    transport = new FakeTransport();
    TestBed.configureTestingModule({
      providers: [provideRouter([]), { provide: GAME_TRANSPORT, useValue: transport }],
    });
    TestBed.inject(GameStore);
    TestBed.tick();
    transport.receive({
      v: 1,
      type: 'session.welcome',
      payload: { sessionToken: 't', playerId: me },
    });
    const fixture = TestBed.createComponent(Lobby);
    fixture.componentRef.setInput('room', roomOf(players));
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    return {
      host,
      fixture,
      query: (selector: string) => host.querySelector<HTMLElement>(selector),
    };
  }

  const startButton = (host: HTMLElement) =>
    Array.from(host.querySelectorAll<HTMLButtonElement>('button')).find((b) =>
      b.textContent?.includes('Lancer la partie'),
    );

  it('dims "Lancer la partie" with aria-disabled, never disabled, and explains why through the tooltip', () => {
    const { host, fixture } = render('lea', [
      member('lea', 'Léa', { isHost: true, isReady: true }),
      member('loic', 'Loïc'),
    ]);
    const start = startButton(host);

    expect(start?.getAttribute('aria-disabled')).toBe('true');
    expect(start?.disabled).toBe(false);
    const tooltip = host.querySelector('[role="tooltip"]');
    expect(start?.getAttribute('aria-describedby')).toBe(tooltip?.id);
    expect(tooltip?.textContent).toContain('En attente de : Loïc');
    expect(host.textContent).not.toContain('En attente de Loïc.');

    start?.click();
    fixture.detectChanges();

    expect(tooltip?.classList.contains('shown')).toBe(true);
    expect(transport.sent.some((m) => m.type === 'match.start')).toBe(false);
  });

  it('says a second player is needed when the host is alone', () => {
    const { host } = render('lea', [member('lea', 'Léa', { isHost: true })]);

    expect(host.querySelector('[role="tooltip"]')?.textContent).toContain('au moins 2 joueurs');
  });

  it('lets the host start once everybody is ready', () => {
    const { host } = render('lea', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc', { isReady: true }),
    ]);
    const start = startButton(host);

    expect(start?.getAttribute('aria-disabled')).toBe('false');
    expect(host.querySelector('[role="tooltip"]')).toBeNull();

    start?.click();

    expect(transport.sent.at(-1)).toMatchObject({ type: 'match.start' });
  });

  it('gives a guest a toggle "Prêt", a read-only summary, and no kick button', () => {
    const { host, query } = render('loic', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc'),
    ]);
    const ready = Array.from(host.querySelectorAll('button')).find((b) =>
      b.textContent?.includes('Prêt'),
    );

    expect(ready?.getAttribute('aria-pressed')).toBe('false');
    expect(startButton(host)).toBeUndefined();
    expect(host.querySelector('app-segmented:not(app-motion-setting app-segmented)')).toBeNull();
    expect(query('.summary')?.textContent).toContain('Partie en 500 points');
    expect(host.textContent).not.toContain('Exclure');

    ready?.click();

    expect(transport.sent.at(-1)).toMatchObject({
      type: 'room.setReady',
      payload: { ready: true },
    });
  });

  it('marks the host with a crown, and the host alone gets the settings form and the kick buttons', () => {
    const { host } = render('lea', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc'),
    ]);

    expect(host.querySelector('[aria-label="Hôte"]')).not.toBeNull();
    expect(host.querySelectorAll('app-segmented').length).toBeGreaterThanOrEqual(5);
    expect(host.querySelector('[aria-label="Exclure Loïc"]')).not.toBeNull();
  });
});
