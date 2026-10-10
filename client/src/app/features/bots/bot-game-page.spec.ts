import { TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { GAME_TRANSPORT } from '../../core/game-transport';
import { GameStore } from '../../state/game-store';
import { FakeTransport } from '../../testing/fake-transport';
import { BotGamePage } from './bot-game-page';

describe('BotGamePage', () => {
  let transport: FakeTransport;

  function render(nickname: string) {
    localStorage.clear();
    sessionStorage.clear();
    localStorage.setItem('uno.nickname', nickname);
    transport = new FakeTransport();
    TestBed.configureTestingModule({
      providers: [provideRouter([]), { provide: GAME_TRANSPORT, useValue: transport }],
    });
    TestBed.inject(GameStore);
    TestBed.tick();
    transport.receive({
      v: 1,
      type: 'session.welcome',
      payload: { sessionToken: 't', playerId: 'me' },
    });
    const fixture = TestBed.createComponent(BotGamePage);
    fixture.detectChanges();
    return { host: fixture.nativeElement as HTMLElement, fixture };
  }

  const radio = (host: HTMLElement, group: string, label: string) =>
    Array.from(host.querySelectorAll<HTMLButtonElement>(`[aria-label="${group}"] button`)).find(
      (b) => b.textContent?.trim() === label,
    );

  it('starts a game against the chosen number of bots, at the chosen level, with the room settings', () => {
    const { host, fixture } = render('Léa');

    radio(host, 'Nombre de bots', '5')?.click();
    radio(host, 'Niveau des bots', 'Facile')?.click();
    radio(host, 'Durée de la partie', 'Manche unique')?.click();
    fixture.detectChanges();
    Array.from(host.querySelectorAll<HTMLButtonElement>('button'))
      .find((b) => b.textContent?.trim() === 'Jouer')
      ?.click();

    const sent = transport.sent.at(-1);
    expect(sent).toMatchObject({
      type: 'room.createBotGame',
      payload: {
        nickname: 'Léa',
        botCount: 5,
        level: 'easy',
        settings: { matchLength: 'singleRound', stacking: 'official' },
      },
    });
    // Le serveur fixe les places : le nombre maximum de joueurs n'est pas envoyé
    expect(JSON.stringify(sent)).not.toContain('maxPlayers');
  });

  it('shows the same game settings as the lobby, but not the maximum number of players', () => {
    const { host } = render('Léa');

    expect(host.querySelector('[aria-label="Règle de pioche"]')).not.toBeNull();
    expect(host.querySelector('[aria-label="Cumul des pénalités"]')).not.toBeNull();
    expect(host.querySelector('[aria-label="Joueurs maximum"]')).toBeNull();
  });

  it('goes back to the home page when there is no valid nickname', () => {
    TestBed.resetTestingModule();
    localStorage.clear();
    const router = { navigate: vi.fn().mockResolvedValue(true) };
    TestBed.configureTestingModule({
      providers: [
        { provide: GAME_TRANSPORT, useValue: new FakeTransport() },
        { provide: Router, useValue: router },
      ],
    });
    TestBed.createComponent(BotGamePage);

    expect(router.navigate).toHaveBeenCalledWith(['/']);
  });
});
