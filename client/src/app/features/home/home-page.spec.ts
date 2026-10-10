import { TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { GAME_TRANSPORT } from '../../core/game-transport';
import { FakeTransport } from '../../testing/fake-transport';
import { GameStore } from '../../state/game-store';
import { HomePage } from './home-page';

describe('HomePage', () => {
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
    const fixture = TestBed.createComponent(HomePage);
    fixture.detectChanges();
    return { host: fixture.nativeElement as HTMLElement, fixture };
  }

  const buttons = (host: HTMLElement) => Array.from(host.querySelectorAll('button'));

  it('offers three choices after the nickname: bots, a new room, an existing room', () => {
    const { host } = render('Léa');

    const titles = Array.from(host.querySelectorAll('.choice strong')).map((e) =>
      e.textContent?.trim(),
    );

    expect(titles).toEqual(['Jouer contre des bots', 'Créer un salon', 'Rejoindre un salon']);
    expect(host.querySelector('input[aria-label="Code du salon"]')).not.toBeNull();
  });

  it('opens the bot screen with the nickname remembered', async () => {
    const { host } = render('Léa');
    const navigate = vi.spyOn(TestBed.inject(Router), 'navigate').mockResolvedValue(true);

    buttons(host)
      .find((b) => b.textContent?.includes('Jouer contre des bots'))
      ?.click();

    expect(navigate).toHaveBeenCalledWith(['/bots']);
    expect(localStorage.getItem('uno.nickname')).toBe('Léa');
  });

  it('keeps the choices out of reach until the nickname is valid', () => {
    const { host } = render('');

    const disabled = buttons(host)
      .filter((b) => b.classList.contains('choice') || b.textContent?.includes('Rejoindre'))
      .map((b) => b.disabled);

    expect(disabled).toEqual([true, true, true]);
  });

  it('creates a room as before', () => {
    const { host } = render('Léa');

    buttons(host)
      .find((b) => b.textContent?.includes('Créer un salon'))
      ?.click();

    expect(transport.sent.at(-1)).toMatchObject({
      type: 'room.create',
      payload: { nickname: 'Léa' },
    });
  });
});
