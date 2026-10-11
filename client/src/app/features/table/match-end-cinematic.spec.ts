import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { AudioService } from '../../audio/audio.service';
import type { SeatView } from '../../protocol/generated/protocol';
import {
  CINEMATIC_MS,
  CINEMATIC_REDUCED_MS,
  MatchEndCinematic,
  scoreAt,
} from './match-end-cinematic';

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
  avatarId: 4,
});

const PLAYERS = [
  seat('a', 'Léa', 120),
  seat('b', 'Max', 510),
  seat('c', 'Zoé', 330),
  seat('d', 'Ali', 40),
];

describe('scoreAt', () => {
  it('counts from zero to the score, whole numbers only, and never beyond', () => {
    expect(scoreAt(510, 0)).toBe(0);
    expect(scoreAt(510, 0.5)).toBe(255);
    expect(scoreAt(333, 0.333)).toBe(111);
    expect(scoreAt(510, 3)).toBe(510);
    expect(scoreAt(510, -1)).toBe(0);
  });
});

describe('MatchEndCinematic', () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => vi.useRealTimers());

  function render(meId: string) {
    const fixture = TestBed.createComponent(MatchEndCinematic);
    fixture.componentRef.setInput('players', PLAYERS);
    fixture.componentRef.setInput('winnerId', 'b');
    fixture.componentRef.setInput('meId', meId);
    const play = vi.spyOn(TestBed.inject(AudioService), 'play');
    let finished = 0;
    fixture.componentInstance.finished.subscribe(() => finished++);
    fixture.detectChanges();
    return { fixture, host: fixture.nativeElement as HTMLElement, play, finished: () => finished };
  }

  it('shows the podium: first in the middle, then second and third, with the others below', () => {
    const { host } = render('a');

    const places = [...host.querySelectorAll('.podium .place')];
    expect(places.map((p) => p.querySelector('.name')?.textContent)).toEqual(['Max', 'Zoé', 'Léa']);
    expect(places.map((p) => p.className)).toEqual([
      'place rank-1',
      'place rank-2',
      'place rank-3',
    ]);
    expect(places.every((p) => p.querySelector('app-avatar') !== null)).toBe(true);
    expect(host.querySelector('.others')?.textContent).toContain('Ali');
    expect(host.querySelector('h2')?.textContent).toContain('Max remporte la partie');
  });

  it('plays the victory sound for the winner and the defeat sound for the others, once', async () => {
    const winner = render('b');
    await winner.fixture.whenStable();
    expect(winner.play).toHaveBeenCalledWith('win');

    const loser = render('a');
    await loser.fixture.whenStable();
    expect(loser.play).toHaveBeenCalledWith('lose');
    expect(loser.play.mock.calls.filter(([sound]) => sound === 'lose')).toHaveLength(1);
    expect(loser.play.mock.calls.filter(([sound]) => sound === 'win')).toHaveLength(1); // celui du gagnant, plus haut
  });

  it('can be skipped with a click, only once', () => {
    const { host, finished } = render('a');

    host.querySelector<HTMLElement>('.cinematic')?.click();
    host.querySelector<HTMLButtonElement>('.skip')?.click();

    expect(finished()).toBe(1);
  });

  it('ends by itself after a few seconds', async () => {
    const { fixture, finished } = render('a');
    await fixture.whenStable();

    vi.advanceTimersByTime(CINEMATIC_MS - 1);
    expect(finished()).toBe(0);
    vi.advanceTimersByTime(2);

    expect(finished()).toBe(1);
    expect(CINEMATIC_REDUCED_MS).toBeLessThan(CINEMATIC_MS);
  });

  it('says whether you won', () => {
    expect(render('b').host.querySelector('.mood')?.textContent).toContain('tu as gagné');
    expect(render('a').host.querySelector('.mood')?.textContent).not.toContain('tu as gagné');
  });
});
