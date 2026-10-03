import { TestBed } from '@angular/core/testing';
import { GAME_TRANSPORT } from '../core/game-transport';
import { FakeTransport } from '../testing/fake-transport';
import type { ServerMessage } from '../protocol/generated/protocol';
import { GameStore } from './game-store';

const welcome = (resumedRoomCode?: string): ServerMessage => ({
  v: 1,
  type: 'session.welcome',
  payload: { sessionToken: 'tok', playerId: 'p1', ...(resumedRoomCode ? { resumedRoomCode } : {}) },
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
    expect(sessionStorage.getItem('uno.sessionToken')).toBe('tok');
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

  it('starts a fresh session when the stored token has expired', async () => {
    sessionStorage.setItem('uno.sessionToken', 'old');
    TestBed.tick();
    transport.receive({
      v: 1,
      type: 'error',
      replyTo: transport.sent[0].id,
      payload: { code: 'SESSION_EXPIRED', message: 'x' },
    });
    await Promise.resolve();

    expect(transport.sent[1]).toMatchObject({ type: 'session.hello' });
    expect(transport.sent[1].payload).not.toHaveProperty('sessionToken');
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
            stacking: 'off',
            jumpIn: false,
            sevenZero: false,
            drawUntilPlayable: false,
            wildDrawFourMode: 'officialChallenge',
            turnTimerSeconds: 0,
            matchLength: 'to500',
            maxPlayers: 6,
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
});
