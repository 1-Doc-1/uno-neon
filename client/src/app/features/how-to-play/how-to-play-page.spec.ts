import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { describe, expect, it } from 'vitest';
import { HowToPlayPage } from './how-to-play-page';

describe('HowToPlayPage', () => {
  function render() {
    TestBed.configureTestingModule({ providers: [provideRouter([])] });
    const fixture = TestBed.createComponent(HowToPlayPage);
    fixture.detectChanges();
    return fixture.nativeElement as HTMLElement;
  }

  it('explains the six special cards, each with its card', () => {
    const host = render();

    const names = [...host.querySelectorAll('.specials strong')].map((n) => n.textContent?.trim());
    expect(names).toEqual(['+2', 'Passe', 'Inversion', 'Joker', 'Joker +4', 'Joker +5 (doré)']);
    expect(host.querySelectorAll('.specials app-card')).toHaveLength(6);
  });

  it('explains every house rule of the lobby with an example', () => {
    const host = render();
    const text = host.textContent ?? '';

    for (const name of [
      'Durée de la partie',
      'Minuteur par tour',
      'Règle de pioche',
      'Pioche',
      'Dernière carte',
      'Cumul des pénalités',
      'Jokers +5',
    ]) {
      expect(text).toContain(name);
    }
    expect(host.querySelectorAll('.rule .example')).toHaveLength(
      host.querySelectorAll('.rule').length,
    );
  });

  it('covers UNO, the counter-UNO and the audio settings, and leads back home', () => {
    const host = render();

    expect(host.textContent).toContain('Contre-UNO');
    expect(host.textContent).toContain('Réglages audio');
    expect(host.querySelectorAll('a[href="/"]').length).toBeGreaterThan(0);
  });
});
