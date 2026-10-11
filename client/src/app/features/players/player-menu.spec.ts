import { Component, signal } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it } from 'vitest';
import { GAME_TRANSPORT } from '../../core/game-transport';
import type { RoomMember, ServerMessage } from '../../protocol/generated/protocol';
import { GameStore } from '../../state/game-store';
import { FakeTransport } from '../../testing/fake-transport';
import { DEFAULT_SETTINGS } from '../room/default-settings';
import { PlayerMenu } from './player-menu';

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
  avatarId: 3,
  ...extra,
});

@Component({
  imports: [PlayerMenu],
  template: `@if (target(); as id) {
    <app-player-menu [playerId]="id" (closed)="target.set(null)" />
  }`,
})
class Host {
  readonly target = signal<string | null>('loic');
}

describe('PlayerMenu', () => {
  let transport: FakeTransport;
  let store: GameStore;

  function render(me: string, players: RoomMember[]) {
    TestBed.tick();
    transport.receive({
      v: 1,
      type: 'session.welcome',
      payload: { playerId: me },
    } as ServerMessage);
    transport.receive({
      v: 1,
      type: 'room.update',
      payload: {
        roomVersion: 1,
        room: { code: 'ABCDEF', phase: 'lobby', settings: DEFAULT_SETTINGS, players },
      },
    });
    const fixture = TestBed.createComponent(Host);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    const button = (text: string) =>
      [...host.querySelectorAll<HTMLButtonElement>('button')].find((b) =>
        b.textContent?.includes(text),
      );
    return { fixture, host, button };
  }

  beforeEach(() => {
    localStorage.clear();
    HTMLDialogElement.prototype.showModal = function showModal(): void {
      this.setAttribute('open', '');
    };
    transport = new FakeTransport();
    TestBed.configureTestingModule({
      providers: [{ provide: GAME_TRANSPORT, useValue: transport }],
    });
    store = TestBed.inject(GameStore);
  });

  it('lets the host exclude a player, after a second step that names them', () => {
    const { fixture, host, button } = render('lea', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc'),
    ]);

    button('Exclure')?.click();
    fixture.detectChanges();

    expect(host.textContent).toContain('Exclure Loïc ?');
    expect(transport.sent.some((m) => m.type === 'room.kick')).toBe(false);

    button('Exclure')?.click();

    expect(transport.sent.at(-1)).toMatchObject({
      type: 'room.kick',
      payload: { playerId: 'loic' },
    });
  });

  it('cancels the exclusion without telling the server', () => {
    const { fixture, host, button } = render('lea', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc'),
    ]);
    button('Exclure')?.click();
    fixture.detectChanges();

    button('Annuler')?.click();
    fixture.detectChanges();

    expect(host.textContent).not.toContain('Exclure Loïc ?');
    expect(transport.sent.some((m) => m.type === 'room.kick')).toBe(false);
  });

  it('offers no exclusion to somebody who is not the host', () => {
    const { host, button } = render('max', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc'),
      member('max', 'Max'),
    ]);

    expect(button('Exclure')).toBeUndefined();
    expect(host.textContent).toContain('Loïc');
  });

  it('hides a player for this browser, and shows them again', () => {
    const { fixture, button } = render('max', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Loïc'),
      member('max', 'Max'),
    ]);

    button('Masquer ce joueur')?.click();
    fixture.detectChanges();

    expect(store.mutedPlayers().has('loic')).toBe(true);
    expect(fixture.nativeElement.textContent).not.toContain('Loïc');
    fixture.componentInstance.target.set('loic');
    fixture.detectChanges();
    expect(button('Réafficher ses messages')).toBeDefined();
  });

  it('does not offer to hide a bot: bots do not write', () => {
    const { button } = render('lea', [
      member('lea', 'Léa', { isHost: true }),
      member('loic', 'Pixel', { isBot: true }),
    ]);

    expect(button('Masquer')).toBeUndefined();
  });
});
