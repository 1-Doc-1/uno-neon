import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { GAME_TRANSPORT } from '../../core/game-transport';
import type { RoomMember, RoomView } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { FakeTransport } from '../../testing/fake-transport';
import { DEFAULT_SETTINGS } from './default-settings';
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

describe('Lobby with bots', () => {
  let transport: FakeTransport;

  function render(me: string, players: RoomMember[], maxPlayers = 6) {
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
    const room: RoomView = {
      code: 'K7M4XP',
      phase: 'lobby',
      settings: { ...DEFAULT_SETTINGS, maxPlayers },
      players,
    };
    const fixture = TestBed.createComponent(Lobby);
    fixture.componentRef.setInput('room', room);
    fixture.detectChanges();
    return fixture.nativeElement as HTMLElement;
  }

  const button = (host: HTMLElement, text: string) =>
    Array.from(host.querySelectorAll<HTMLButtonElement>('button')).find((b) =>
      b.textContent?.includes(text),
    );

  it('marks a bot with a badge, and lets the host remove it in one click', () => {
    const host = render('lea', [
      member('lea', 'Léa', { isHost: true }),
      member('bot-1', 'Nova', { isBot: true, isReady: true }),
    ]);

    expect(host.querySelector('.bot-badge')?.textContent?.trim()).toBe('Bot');
    host.querySelector<HTMLButtonElement>('button[aria-label="Retirer Nova"]')?.click();

    expect(transport.sent.at(-1)).toMatchObject({
      type: 'room.kick',
      payload: { playerId: 'bot-1' },
    });
  });

  it('lets the host add a bot of the chosen level', () => {
    const host = render('lea', [member('lea', 'Léa', { isHost: true })]);

    button(host, 'Ajouter un bot')?.click();
    expect(transport.sent.at(-1)).toMatchObject({
      type: 'room.addBot',
      payload: { level: 'normal' },
    });

    Array.from(host.querySelectorAll<HTMLButtonElement>('[aria-label="Niveau du bot"] button'))
      .find((b) => b.textContent?.trim() === 'Facile')
      ?.click();
    TestBed.tick();
    button(host, 'Ajouter un bot')?.click();
    expect(transport.sent.at(-1)).toMatchObject({
      type: 'room.addBot',
      payload: { level: 'easy' },
    });
  });

  it('does not offer a bot when the room is full, nor to a guest', () => {
    const full = render(
      'lea',
      [member('lea', 'Léa', { isHost: true }), member('bot-1', 'Nova', { isBot: true })],
      2,
    );
    expect(button(full, 'Ajouter un bot')?.disabled).toBe(true);
    TestBed.resetTestingModule();

    const guest = render('max', [member('lea', 'Léa', { isHost: true }), member('max', 'Max')]);
    expect(button(guest, 'Ajouter un bot')).toBeUndefined();
  });
});
