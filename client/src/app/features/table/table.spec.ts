import { TestBed } from '@angular/core/testing';
import { GAME_TRANSPORT } from '../../core/game-transport';
import type { PlayerView, SeatView, UnoWindow } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { FakeTransport } from '../../testing/fake-transport';
import { Table } from './table';

const seat = (playerId: string, nickname: string, cardCount: number): SeatView => ({
  playerId,
  nickname,
  seat: ['me', 'max', 'lea'].indexOf(playerId),
  cardCount,
  score: 0,
  isConnected: true,
  isBot: false,
  isHost: false,
  hasCalledUno: false,
  isReadyForNextRound: false,
});

function viewWith(
  overrides: Partial<PlayerView> = {},
  me: Partial<PlayerView['me']> = {},
): PlayerView {
  return {
    stateVersion: 1,
    phase: 'awaitingPlay',
    me: {
      playerId: 'me',
      hand: [{ id: 1, color: 'red', rank: '5' }],
      playableCardIds: [],
      canDraw: false,
      canKeepDrawnCard: false,
      mustDeclareUno: false,
      canCallUno: false,
      canChooseColor: false,
      penaltyResponse: null,
      ...me,
    },
    players: [seat('me', 'Moi', 1), seat('max', 'Max', 1), seat('lea', 'Léa', 5)],
    currentPlayerId: 'lea',
    direction: 'clockwise',
    currentColor: 'red',
    discardTop: { id: 2, color: 'red', rank: '2' },
    drawPileCount: 40,
    pendingDraw: 0,
    turnDeadline: null,
    nextRoundDeadline: null,
    unoWindows: [],
    round: 1,
    settings: {
      stacking: 'off',
      jumpIn: false,
      sevenZero: false,
      drawUntilPlayable: false,
      wildDrawFourMode: 'officialChallenge',
      turnTimerSeconds: 0,
      matchLength: 'to500',
      maxPlayers: 6,
      drawRule: 'guided',
      declareUnoToWin: false,
    },
    roundResult: null,
    matchWinnerId: null,
    ...overrides,
  };
}

describe('Table', () => {
  let transport: FakeTransport;

  function render(view: PlayerView) {
    TestBed.configureTestingModule({
      providers: [{ provide: GAME_TRANSPORT, useValue: transport }],
    });
    TestBed.inject(GameStore);
    TestBed.tick();
    transport.receive({
      v: 1,
      type: 'session.welcome',
      payload: { sessionToken: 'tok', playerId: 'me' },
    });
    const fixture = TestBed.createComponent(Table);
    fixture.componentRef.setInput('view', view);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    return {
      fixture,
      host,
      button: (selector: string) => host.querySelector<HTMLButtonElement>(selector),
    };
  }

  const window = (targetId: string, graceEndsAt: number, expiresAt: number): UnoWindow => ({
    targetId,
    graceEndsAt,
    expiresAt,
  });

  beforeEach(() => {
    sessionStorage.clear();
    transport = new FakeTransport();
  });

  it('offers the counter-UNO against another player even when it is not my turn, with name and countdown', () => {
    const now = Date.now();
    const { button } = render(viewWith({ unoWindows: [window('max', now - 500, now + 9_500)] }));

    const catchButton = button('.catch');

    expect(catchButton).not.toBeNull();
    expect(catchButton?.textContent).toContain('Contre-UNO !');
    expect(catchButton?.textContent).toContain('Max');
    expect(catchButton?.textContent).toMatch(/(9|10) s/);

    catchButton?.click();

    expect(transport.sent.at(-1)).toMatchObject({
      type: 'game.catchUno',
      payload: { targetId: 'max' },
    });
  });

  it('greys the counter-UNO out during the grace period: still there, but a click does nothing', () => {
    const now = Date.now();
    const { button } = render(viewWith({ unoWindows: [window('max', now + 1_500, now + 14_500)] }));

    const catchButton = button('.catch');

    expect(catchButton?.getAttribute('aria-disabled')).toBe('true');
    expect(catchButton?.textContent).toMatch(/dans (1|2) s/);
    expect(catchButton?.disabled).toBe(false); // atténué, pas désactivé : il reste focalisable

    catchButton?.click();

    expect(transport.sent.some((m) => m.type === 'game.catchUno')).toBe(false);
  });

  it('offers one counter-UNO per target, each with its own countdown', () => {
    const now = Date.now();
    const { host } = render(
      viewWith({
        players: [seat('me', 'Moi', 5), seat('max', 'Max', 1), seat('lea', 'Léa', 1)],
        unoWindows: [
          window('max', now - 500, now + 9_500),
          window('lea', now + 1_000, now + 14_000),
        ],
      }),
    );

    const labels = Array.from(host.querySelectorAll('.catch')).map((b) =>
      b.textContent?.replace(/\s+/g, ' ').trim(),
    );

    expect(labels).toHaveLength(2);
    expect(labels[0]).toContain('Max');
    expect(labels[1]).toContain('Léa');
    expect(labels[1]).toContain('dans');
  });

  it('shows no counter-UNO on myself, nor once the window ran out', () => {
    const now = Date.now();
    const { button } = render(
      viewWith({
        unoWindows: [
          window('me', now - 500, now + 9_500),
          window('lea', now - 20_000, now - 5_000),
        ],
      }),
    );

    expect(button('.catch')).toBeNull();
  });

  it('flags the player who forgot UNO on their seat while their window is open', () => {
    const now = Date.now();
    const { host } = render(viewWith({ unoWindows: [window('max', now - 500, now + 9_500)] }));

    expect(host.textContent).toContain('UNO oublié !');
  });

  it('has no draw or pass button: the deck draws when the server allows it', () => {
    const { host, button, fixture } = render(viewWith({}, { canDraw: true }));

    const labels = Array.from(host.querySelectorAll('button')).map((b) => b.textContent?.trim());
    expect(labels).not.toContain('Piocher');
    expect(labels).not.toContain('Passer');

    const deck = button('.deck');
    expect(deck?.getAttribute('aria-label')).toBe('Piocher');
    expect(deck?.disabled).toBe(false);
    deck?.click();
    expect(transport.sent.at(-1)).toMatchObject({ type: 'game.drawCard' });

    fixture.componentRef.setInput('view', viewWith({}, { canKeepDrawnCard: true }));
    fixture.detectChanges();
    expect(button('.deck')?.getAttribute('aria-label')).toBe('Garder la carte');
    button('.deck')?.click();
    expect(transport.sent.at(-1)).toMatchObject({ type: 'game.pass' });
  });

  it('leaves the deck inert when the server says neither draw nor keep', () => {
    const { button } = render(viewWith());

    expect(button('.deck')?.disabled).toBe(true);
    expect(button('.deck')?.getAttribute('aria-label')).toBe('Pioche : 40 cartes');
  });

  it('puts the UNO button forward when the last card waits for the announcement', () => {
    const { host, button } = render(viewWith({}, { canCallUno: true, mustDeclareUno: true }));

    expect(button('.uno')?.classList.contains('must')).toBe(true);
    expect(host.textContent).toContain('Annonce UNO pour poser ta dernière carte.');

    button('.uno')?.click();

    expect(transport.sent.at(-1)).toMatchObject({ type: 'game.callUno' });
  });

  it('keeps the UNO button discreet when nothing is waiting for it', () => {
    const { host, button } = render(viewWith({}, { canCallUno: true }));

    expect(button('.uno')?.classList.contains('must')).toBe(false);
    expect(host.textContent).not.toContain('Annonce UNO pour poser');
  });

  it('shows the UNO button only when it is useful', () => {
    const { button, fixture } = render(viewWith());
    expect(button('.uno')).toBeNull();

    fixture.componentRef.setInput('view', viewWith({}, { canCallUno: true }));
    fixture.detectChanges();

    expect(button('.uno')).not.toBeNull();
  });
});
