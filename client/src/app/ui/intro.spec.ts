import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { AUDIO_CONTEXT_FACTORY, AudioService } from '../audio/audio.service';
import { Intro } from './intro';

describe('Intro', () => {
  beforeEach(() => {
    sessionStorage.clear();
    localStorage.clear();
    vi.useFakeTimers();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => null }],
    });
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  /** `begun` : le clic sur « Cliquer pour jouer » est déjà fait, la cinématique tourne. */
  function render(begun = true) {
    const fixture = TestBed.createComponent(Intro);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    const gate = () => host.querySelector<HTMLButtonElement>('.gate-button');
    if (begun) {
      gate()?.click();
      fixture.detectChanges();
    }
    return { fixture, host, gate, shown: () => host.querySelector('.intro') };
  }

  it('plays on the first visit: the logo and a fan of cards, built from CSS and SVG only', () => {
    const { host, shown } = render();

    expect(shown()).not.toBeNull();
    expect(host.querySelector('app-logo')).not.toBeNull();
    expect(host.querySelectorAll('.flyer').length).toBeGreaterThanOrEqual(5);
    expect(host.querySelector('img, audio, video, link')).toBeNull();
  });

  it('first asks for a click, and shows nothing of the opening before it', () => {
    const { host, gate, shown } = render(false);

    expect(gate()?.textContent).toContain('Cliquer pour jouer');
    expect(shown()).toBeNull();
    expect(host.querySelectorAll('.flyer')).toHaveLength(0);
    vi.advanceTimersByTime(10_000);
    expect(gate()).not.toBeNull();
  });

  it('a click on the gate unlocks the audio and plays the opening sound, once', () => {
    const audio = TestBed.inject(AudioService);
    const unlock = vi.spyOn(audio, 'unlock');
    const play = vi.spyOn(audio, 'play');
    const { fixture, gate, shown } = render(false);

    gate()?.click();
    fixture.detectChanges();

    expect(unlock).toHaveBeenCalled();
    expect(play).toHaveBeenCalledWith('intro');
    expect(play.mock.calls.filter(([sound]) => sound === 'intro')).toHaveLength(1);
    expect(gate()).toBeNull();
    expect(shown()).not.toBeNull();
  });

  it('does not ask again once the session has seen it', () => {
    render(false);
    TestBed.resetTestingModule();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => null }],
    });

    expect(render(false).gate()).toBeNull();
  });

  it('plays once per session', () => {
    render();
    TestBed.resetTestingModule();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => null }],
    });

    expect(render().shown()).toBeNull();
    expect(sessionStorage.getItem('uno.introSeen')).toBe('1');
  });

  it('is over by itself after 2 to 3 seconds', () => {
    const { fixture, shown } = render();

    vi.advanceTimersByTime(2400);
    expect(shown()).not.toBeNull();
    vi.advanceTimersByTime(600);
    fixture.detectChanges();

    expect(shown()).toBeNull();
  });

  it('can be skipped with a click', () => {
    const { fixture, host, shown } = render();

    host.click();
    fixture.detectChanges();
    expect(shown()?.classList.contains('leaving')).toBe(true);
    vi.advanceTimersByTime(300);
    fixture.detectChanges();

    expect(shown()).toBeNull();
  });

  it('can be skipped with a key', () => {
    const { fixture, shown } = render();

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' }));
    fixture.detectChanges();
    vi.advanceTimersByTime(300);
    fixture.detectChanges();

    expect(shown()).toBeNull();
  });

  it('is a simple, shorter fade when motion is reduced', () => {
    vi.stubGlobal('matchMedia', (query: string) => ({
      matches: query.includes('reduce'),
      addEventListener: () => undefined,
      removeEventListener: () => undefined,
    }));
    const { fixture, shown } = render();

    expect(shown()?.classList.contains('reduced')).toBe(true);
    vi.advanceTimersByTime(1500);
    fixture.detectChanges();

    expect(shown()).toBeNull();
  });

  it('is not played to a browser driven by a robot', () => {
    vi.stubGlobal('navigator', { webdriver: true });

    expect(render().gate()).toBeNull();
    expect(render().shown()).toBeNull();
  });

  it('is hidden from assistive technologies: it is only decoration', () => {
    expect(render().shown()?.getAttribute('aria-hidden')).toBe('true');
  });
});
