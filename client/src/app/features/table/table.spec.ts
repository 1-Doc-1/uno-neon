import { TestBed } from '@angular/core/testing';
import { GAME_TRANSPORT } from '../../core/game-transport';
import type { Card, PlayerView, SeatView, UnoWindow } from '../../protocol/generated/protocol';
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
    actionsOpenAt: 0,
    drawStepMs: 1000,
    unoWindows: [],
    round: 1,
    settings: {
      stacking: 'official',
      jumpIn: false,
      sevenZero: false,
      drawAmount: 'untilPlayable',
      wildDrawFourMode: 'officialChallenge',
      turnTimerSeconds: 0,
      matchLength: 'to500',
      maxPlayers: 6,
      drawRule: 'guided',
      declareUnoToWin: false,
      drawTwoMultiplier: 1,
      wildDrawFourMultiplier: 1,
      wildDrawFiveMultiplier: 1,
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

  describe('while the effect in progress is shown (actionsOpenAt in the future)', () => {
    const closed = (): number => Date.now() + 60_000;
    const mine = { canDraw: true, playableCardIds: [1] };

    it('leaves my hand neutral and the deck inert, then opens them at the time the server gave', () => {
      const { button, host, fixture } = render(
        viewWith({ currentPlayerId: 'me', actionsOpenAt: closed() }, mine),
      );

      expect(button('.deck')?.disabled).toBe(true);
      expect(host.querySelector<HTMLButtonElement>('.slot')?.disabled).toBe(true);
      expect(host.querySelector('.slot.playable')).toBeNull();

      fixture.componentRef.setInput(
        'view',
        viewWith({ currentPlayerId: 'me', actionsOpenAt: 0 }, mine),
      );
      fixture.detectChanges();

      expect(button('.deck')?.disabled).toBe(false);
      expect(host.querySelector<HTMLButtonElement>('.slot')?.disabled).toBe(false);
    });

    it('holds back the answer to a penalty, which the server would refuse', () => {
      const penalty = { amount: 4, canChallenge: true, canStack: false };
      const { host, fixture } = render(
        viewWith({ currentPlayerId: 'me', actionsOpenAt: closed() }, { penaltyResponse: penalty }),
      );

      expect(host.querySelector('app-challenge-dialog')).toBeNull();

      fixture.componentRef.setInput(
        'view',
        viewWith({ currentPlayerId: 'me', actionsOpenAt: 0 }, { penaltyResponse: penalty }),
      );
      fixture.detectChanges();

      expect(host.querySelector('app-challenge-dialog')).not.toBeNull();
    });

    it('shows the cards that answer a penalty as soon as it reaches me, but keeps them locked until the server opens', () => {
      const penalty = { amount: 5, canChallenge: false, canStack: true };
      const answering = { playableCardIds: [1], penaltyResponse: penalty };
      const { host, fixture } = render(
        viewWith({ currentPlayerId: 'me', actionsOpenAt: closed() }, answering),
      );

      const slot = host.querySelector<HTMLButtonElement>('.slot.playable');
      expect(slot).not.toBeNull();
      expect(slot?.disabled).toBe(true);
      expect(host.querySelector('app-plus-five-prompt')).not.toBeNull();
      expect(host.querySelector<HTMLButtonElement>('app-plus-five-prompt button')?.disabled).toBe(
        true,
      );

      fixture.componentRef.setInput(
        'view',
        viewWith({ currentPlayerId: 'me', actionsOpenAt: 0 }, answering),
      );
      fixture.detectChanges();

      expect(host.querySelector<HTMLButtonElement>('.slot.playable')?.disabled).toBe(false);
      expect(host.querySelector<HTMLButtonElement>('app-plus-five-prompt button')?.disabled).toBe(
        false,
      );
    });

    it('still lets me announce UNO and catch someone', () => {
      const soon = Date.now() + 1000;
      const { host } = render(
        viewWith(
          {
            actionsOpenAt: closed(),
            unoWindows: [window('max', soon - 500, soon + 14_000)],
          },
          { canCallUno: true },
        ),
      );

      expect(host.querySelector('app-uno-actions')).not.toBeNull();
    });
  });

  describe('the Wild Draw Five', () => {
    const plusFive: Card = { id: 50, color: null, rank: 'wildDrawFive' };
    const mineToPlay: Partial<PlayerView['me']> = {
      hand: [plusFive, { id: 1, color: 'red', rank: '5' }],
      playableCardIds: [50],
    };
    const myTurn: Partial<PlayerView> = { currentPlayerId: 'me' };

    beforeEach(() => {
      HTMLDialogElement.prototype.showModal = function showModal(): void {
        this.setAttribute('open', '');
      };
      HTMLDialogElement.prototype.show = function show(): void {
        this.setAttribute('open', '');
      };
    });

    const click = (host: HTMLElement, selector: string, index = 0): void =>
      host.querySelectorAll<HTMLElement>(selector)[index]?.click();

    it('asks for a target, then a colour, then plays the card with both', () => {
      const { host, fixture } = render(viewWith(myTurn, mineToPlay));

      click(host, '.slot'); // the Wild Draw Five
      fixture.detectChanges();
      expect(host.querySelector('app-target-picker')).not.toBeNull();
      expect(host.querySelector('app-color-picker')).toBeNull();

      click(host, 'app-target-picker .target', 1); // Léa
      fixture.detectChanges();
      expect(host.querySelector('app-target-picker')).toBeNull();
      expect(host.querySelector('app-color-picker')).not.toBeNull();

      click(host, 'app-color-picker .tile', 2);
      fixture.detectChanges();

      expect(transport.sent.at(-1)).toMatchObject({
        type: 'game.playCard',
        payload: { cardId: 50, chosenColor: 'green', targetId: 'lea' },
      });
      expect(host.querySelector('app-color-picker')).toBeNull();
    });

    it('also takes the target from a click on a seat of the table', () => {
      const { host, fixture } = render(viewWith(myTurn, mineToPlay));
      expect(host.querySelector('app-opponent-seat .seat.targetable')).toBeNull();

      click(host, '.slot');
      fixture.detectChanges();
      const seats = host.querySelectorAll<HTMLElement>('app-opponent-seat .seat.targetable');
      expect(seats).toHaveLength(2);
      expect(seats[0].getAttribute('role')).toBe('button');
      expect(seats[0].getAttribute('aria-label')).toBe('Viser Max');
      seats[0].click();
      fixture.detectChanges();

      expect(host.querySelector('app-color-picker')).not.toBeNull();
      click(host, 'app-color-picker .tile', 0);

      expect(transport.sent.at(-1)).toMatchObject({
        type: 'game.playCard',
        payload: { cardId: 50, chosenColor: 'red', targetId: 'max' },
      });
      fixture.detectChanges();
      expect(host.querySelector('app-opponent-seat .seat.targetable')).toBeNull();
    });

    it('plays nothing when the target is cancelled', () => {
      const { host, fixture } = render(viewWith(myTurn, mineToPlay));
      const sent = transport.sent.length;

      click(host, '.slot');
      fixture.detectChanges();
      click(host, 'app-target-picker button[appButton]'); // Annuler
      fixture.detectChanges();

      expect(host.querySelector('app-target-picker')).toBeNull();
      expect(host.querySelector('app-color-picker')).toBeNull();
      expect(transport.sent).toHaveLength(sent);
    });

    it('does not offer to challenge a Wild Draw Five aimed at me: accept, or answer with one', () => {
      const penalty = { amount: 5, canChallenge: false, canStack: true };
      const { host } = render(viewWith(myTurn, { ...mineToPlay, penaltyResponse: penalty }));

      expect(host.querySelector('app-challenge-dialog')).toBeNull();
      expect(host.querySelector('app-plus-five-prompt')?.textContent).toContain('piocher 5');

      click(host, 'app-plus-five-prompt button');

      expect(transport.sent.at(-1)).toMatchObject({
        type: 'game.respondPenalty',
        payload: { response: 'accept' },
      });
    });

    it('lets me answer a Wild Draw Five with the one in my hand', () => {
      const penalty = { amount: 5, canChallenge: false, canStack: true };
      const { host, fixture } = render(
        viewWith(myTurn, { ...mineToPlay, penaltyResponse: penalty }),
      );

      click(host, '.slot');
      fixture.detectChanges();
      click(host, 'app-target-picker .target', 0);
      fixture.detectChanges();
      click(host, 'app-color-picker .tile', 3);

      expect(transport.sent.at(-1)).toMatchObject({
        type: 'game.playCard',
        payload: { cardId: 50, chosenColor: 'blue', targetId: 'max' },
      });
    });

    it('with the ladder there is no window: the pile takes everything that is owed', () => {
      const penalty = { amount: 6, canChallenge: false, canStack: true };
      const { host, button } = render(
        viewWith(
          { ...myTurn, pendingDraw: 6 },
          { ...mineToPlay, canDraw: true, penaltyResponse: penalty },
        ),
      );

      expect(host.querySelector('app-plus-five-prompt')).toBeNull();
      expect(host.querySelector('app-challenge-dialog')).toBeNull();
      expect(host.querySelector('.slot.playable')).not.toBeNull();
      expect(button('.deck')?.getAttribute('aria-label')).toBe('Piocher 6 cartes');

      button('.deck')?.click();

      expect(transport.sent.at(-1)).toMatchObject({ type: 'game.drawCard' });
    });

    it('keeps the +4 challenge dialog for a Wild Draw Four', () => {
      const penalty = { amount: 4, canChallenge: true, canStack: false };
      const { host } = render(viewWith(myTurn, { penaltyResponse: penalty }));

      expect(host.querySelector('app-challenge-dialog')).not.toBeNull();
      expect(host.querySelector('app-plus-five-prompt')).toBeNull();
    });
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
