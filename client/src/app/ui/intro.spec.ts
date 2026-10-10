import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { Intro } from './intro';

describe('Intro', () => {
  beforeEach(() => {
    sessionStorage.clear();
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  function render() {
    const fixture = TestBed.createComponent(Intro);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    return { fixture, host, shown: () => host.querySelector('.intro') };
  }

  it('plays on the first visit: the logo and a fan of cards, built from CSS and SVG only', () => {
    const { host, shown } = render();

    expect(shown()).not.toBeNull();
    expect(host.querySelector('app-logo')).not.toBeNull();
    expect(host.querySelectorAll('.flyer').length).toBeGreaterThanOrEqual(5);
    expect(host.querySelector('img, audio, video, link')).toBeNull();
  });

  it('plays once per session', () => {
    render();
    TestBed.resetTestingModule();

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

    expect(render().shown()).toBeNull();
  });

  it('is hidden from assistive technologies: it is only decoration', () => {
    expect(render().shown()?.getAttribute('aria-hidden')).toBe('true');
  });
});
