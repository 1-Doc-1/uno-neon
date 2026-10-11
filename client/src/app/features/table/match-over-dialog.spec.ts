import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it } from 'vitest';
import type { SeatView } from '../../protocol/generated/protocol';
import { MatchOverDialog } from './match-over-dialog';

const seat = (playerId: string, nickname: string, score: number): SeatView => ({
  playerId,
  nickname,
  seat: 0,
  cardCount: 0,
  score,
  isConnected: true,
  isBot: false,
  isHost: false,
  hasCalledUno: false,
  isReadyForNextRound: false,
  avatarId: 2,
});

describe('MatchOverDialog', () => {
  beforeEach(() => {
    HTMLDialogElement.prototype.showModal = function showModal(): void {
      this.setAttribute('open', '');
    };
  });

  function render(isHost: boolean) {
    const fixture = TestBed.createComponent(MatchOverDialog);
    fixture.componentRef.setInput('players', [seat('a', 'Léa', 120), seat('b', 'Max', 510)]);
    fixture.componentRef.setInput('winnerId', 'b');
    fixture.componentRef.setInput('meId', 'a');
    fixture.componentRef.setInput('isHost', isHost);
    const events: string[] = [];
    fixture.componentInstance.rematch.subscribe(() => events.push('rematch'));
    fixture.componentInstance.backToLobby.subscribe(() => events.push('lobby'));
    fixture.componentInstance.left.subscribe(() => events.push('left'));
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    // La cinématique passe d'un clic ; l'écran des choix vient après
    host.querySelector<HTMLButtonElement>('.skip')?.click();
    fixture.detectChanges();
    const button = (text: string) =>
      [...host.querySelectorAll<HTMLButtonElement>('button')].find((b) =>
        b.textContent?.includes(text),
      );
    return { host, button, events };
  }

  it('ranks the players, winner first', () => {
    const { host } = render(false);

    expect(host.querySelector('h2')?.textContent).toContain('Max remporte la partie');
    expect([...host.querySelectorAll('.ranking .name')].map((n) => n.textContent)).toEqual([
      'Max',
      'Léa',
    ]);
  });

  it('gives the host a rematch and a way back to the lobby, nothing automatic', () => {
    const { button, events } = render(true);

    button('Revanche')?.click();
    button('Retour au salon')?.click();

    expect(events).toEqual(['rematch', 'lobby']);
  });

  it('makes the others wait for the host', () => {
    const { host, button } = render(false);

    expect(host.textContent).toContain('En attente de l’hôte');
    expect(button('Revanche')).toBeUndefined();
    expect(button('Retour au salon')).toBeUndefined();
  });
});
