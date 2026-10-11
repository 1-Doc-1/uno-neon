import { TestBed } from '@angular/core/testing';
import { GAME_TRANSPORT } from '../core/game-transport';
import { FakeTransport } from '../testing/fake-transport';
import type { ClientEvent, ServerMessage } from '../protocol/generated/protocol';
import { scenarioView } from '../dev/fixtures';
import { GameStore } from './game-store';

const welcome = (resumedRoomCode?: string): ServerMessage => ({
  v: 1,
  type: 'session.welcome',
  payload: { playerId: 'p1', ...(resumedRoomCode ? { resumedRoomCode } : {}) },
});

describe('GameStore', () => {
  let transport: FakeTransport;
  let store: GameStore;

  async function connect(): Promise<void> {
    TestBed.tick();
    transport.receive(welcome());
  }

  beforeEach(() => {
    sessionStorage.clear();
    transport = new FakeTransport();
    TestBed.configureTestingModule({
      providers: [{ provide: GAME_TRANSPORT, useValue: transport }],
    });
    store = TestBed.inject(GameStore);
  });

  it('opens the session with a hello and becomes ready on welcome', async () => {
    await connect();

    expect(transport.sent[0]).toMatchObject({ type: 'session.hello' });
    expect(store.ready()).toBe(true);
  });

  it('resolves a request on ack and on error, translating the error to French', async () => {
    await connect();

    const accepted = store.setReady(true);
    transport.receive({ v: 1, type: 'ack', replyTo: transport.sent[1].id });
    expect(await accepted).toBe(true);

    const refused = store.startMatch();
    transport.receive({
      v: 1,
      type: 'error',
      replyTo: transport.sent[2].id,
      payload: { code: 'NOT_ENOUGH_PLAYERS', message: 'x' },
    });
    expect(await refused).toBe(false);
    expect(store.notices()[0].text).toContain('au moins 2 joueurs');
  });

  it('refuses to send requests before the session is established', async () => {
    TestBed.tick();

    expect(await store.startMatch()).toBe(false);
    expect(transport.sent).toHaveLength(1);
  });

  it('says nothing of a token in the hello: the session is an HttpOnly cookie', async () => {
    await connect();

    expect(transport.sent[0].payload).toEqual({ clientVersion: '0.1.0' });
  });

  it('waits when the session is open in another tab, until the player plays here', async () => {
    TestBed.tick();
    transport.receive({ v: 1, type: 'session.busy', payload: {} });

    expect(store.busy()).toBe(true);
    expect(store.ready()).toBe(false);

    store.takeOver();
    expect(transport.sent[1]).toMatchObject({ type: 'session.takeOver' });
    transport.receive(welcome());
    expect(store.busy()).toBe(false);
    expect(store.ready()).toBe(true);
  });

  it('comes back after being taken over: reconnects, and takes the session as soon as it is busy', () => {
    TestBed.tick();
    transport.receive(welcome());
    transport.statusState.set('closed');
    transport.takenOverState.set(true);
    TestBed.tick();
    expect(store.takenOver()).toBe(true);

    store.takeOver();
    TestBed.tick();
    expect(transport.sent.at(-1)).toMatchObject({ type: 'session.hello' });
    transport.receive({ v: 1, type: 'session.busy', payload: {} });

    expect(transport.sent.at(-1)).toMatchObject({ type: 'session.takeOver' });
    expect(store.busy()).toBe(false);
  });

  it('sends the avatar of the browser with the requests that seat the player', async () => {
    localStorage.setItem('uno.avatar', '7');
    await connect();

    void store.createRoom('Léa');
    void store.joinRoom('ABCDEF', 'Léa');

    expect(transport.sent[1]).toMatchObject({ type: 'room.create', payload: { avatarId: 7 } });
    expect(transport.sent[2]).toMatchObject({ type: 'room.join', payload: { avatarId: 7 } });
  });

  describe('chat', () => {
    const line = (playerId: string, text: string): ServerMessage => ({
      v: 1,
      type: 'chat.message',
      payload: { playerId, text },
    });

    it('keeps the lines in order and counts the unread ones until the panel is open', async () => {
      await connect();

      transport.receive(line('p2', 'Salut'));
      transport.receive(line('p1', 'Coucou'));

      expect(store.chat().map((l) => l.text)).toEqual(['Salut', 'Coucou']);
      expect(store.chatUnread()).toBe(2);
      store.setChatOpen(true);
      expect(store.chatUnread()).toBe(0);
      transport.receive(line('p2', 'Encore'));
      expect(store.chatUnread()).toBe(0);
    });

    it('hides a muted player, now and later, for this browser only', async () => {
      await connect();
      transport.receive(line('p2', 'Ennuyeux'));
      transport.receive(line('p3', 'Sympa'));

      store.setMuted('p2', true);
      transport.receive(line('p2', 'Encore moi'));

      expect(store.chat().map((l) => l.text)).toEqual(['Sympa']);
      store.setMuted('p2', false);
      transport.receive(line('p2', 'Revoilà'));
      expect(store.chat().map((l) => l.text)).toEqual(['Sympa', 'Revoilà']);
    });

    it('sends the text as it was typed and shows the server error when it is refused', async () => {
      await connect();

      const refused = store.sendChat('x');
      transport.receive({
        v: 1,
        type: 'error',
        replyTo: transport.sent[1].id,
        payload: { code: 'CHAT_INVALID', message: 'x' },
      });

      expect(transport.sent[1]).toMatchObject({ type: 'chat.send', payload: { text: 'x' } });
      expect(await refused).toBe(false);
      expect(store.notices()[0].text).toContain('Message refusé');
    });

    it('forgets the chat when the room is left', async () => {
      await connect();
      transport.receive(line('p2', 'Salut'));

      const left = store.leaveRoom();
      transport.receive({ v: 1, type: 'ack', replyTo: transport.sent[1].id });
      await left;

      expect(store.chat()).toEqual([]);
    });
  });

  it('asks the server to go back to the lobby, or to play again', async () => {
    await connect();

    void store.backToLobby();
    void store.rematch();

    expect(transport.sent[1]).toMatchObject({ type: 'room.backToLobby' });
    expect(transport.sent[2]).toMatchObject({ type: 'match.rematch' });
  });

  it('ignores a room update older than the one already shown', async () => {
    await connect();
    const room = (nickname: string, roomVersion: number): ServerMessage => ({
      v: 1,
      type: 'room.update',
      payload: {
        roomVersion,
        room: {
          code: 'ABCDEF',
          phase: 'lobby',
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
            animationSpeed: 'normal',
            drawTwoMultiplier: 1,
            wildDrawFourMultiplier: 1,
            wildDrawFiveMultiplier: 1,
          },
          players: [
            {
              playerId: 'p1',
              nickname,
              seat: 0,
              isHost: true,
              isReady: true,
              isConnected: true,
              isBot: false,
              avatarId: 0,
            },
          ],
        },
      },
    });
    transport.receive(room('Récent', 5));
    transport.receive(room('Ancien', 4));

    expect(store.room()?.players[0].nickname).toBe('Récent');
    expect(store.isHost()).toBe(true);
  });

  it('forgets the room when it is closed', async () => {
    await connect();
    transport.receive({ v: 1, type: 'room.closed', payload: { reason: 'kicked' } });

    expect(store.room()).toBeNull();
    expect(store.closedReason()).toBe('kicked');
  });

  describe('event batches for the animations', () => {
    const update = (stateVersion: number, events: ClientEvent[] = []): ServerMessage => ({
      v: 1,
      type: 'game.update',
      payload: {
        serverTime: Date.now(),
        events,
        view: { ...scenarioView('players-3', 0), stateVersion },
      },
    });
    const turn: ClientEvent = { kind: 'turnChanged', playerId: 'o0' };

    it('replays consecutive updates and calls the first view, a gap or a reconnection a full view', async () => {
      await connect();

      transport.receive(update(10, [turn]));
      expect(store.eventBatch()).toMatchObject({ resync: true, events: [turn] });

      transport.receive(update(11, [turn]));
      expect(store.eventBatch()).toMatchObject({ resync: false });

      transport.receive(update(14, [turn]));
      expect(store.eventBatch()).toMatchObject({ resync: true });

      transport.receive(update(15, [turn]));
      expect(store.eventBatch()).toMatchObject({ resync: false });

      transport.statusState.set('closed');
      TestBed.tick();
      transport.statusState.set('open');
      TestBed.tick();
      transport.receive(update(16, [turn]));
      expect(store.eventBatch()).toMatchObject({ resync: true });
    });

    it('does not offer a batch again for a view it already showed', async () => {
      await connect();
      transport.receive(update(10));
      const first = store.eventBatch();

      transport.receive(update(10, [turn]));

      expect(store.eventBatch()).toBe(first);
    });
  });
});
