import { TestBed } from '@angular/core/testing';
import { APP_NAME } from '../core/app-name';
import { ThemeService } from '../core/theme.service';
import { Icon, IconName } from './icon';
import { Logo } from './logo';

describe('Logo', () => {
  it('draws one tile per letter of the app name and is named after it', () => {
    const fixture = TestBed.createComponent(Logo);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;

    expect(host.querySelector('svg')?.getAttribute('aria-label')).toBe(APP_NAME);
    expect(
      Array.from(host.querySelectorAll('text'))
        .map((t) => t.textContent?.trim())
        .join(''),
    ).toBe(APP_NAME);
  });
});

describe('Icon', () => {
  const names: IconName[] = [
    'door',
    'crown',
    'copy',
    'check',
    'close',
    'arrow-cw',
    'arrow-ccw',
    'palette',
  ];

  it('draws something for every icon, decoratively, without any raw HTML', () => {
    for (const name of names) {
      const fixture = TestBed.createComponent(Icon);
      fixture.componentRef.setInput('name', name);
      fixture.detectChanges();
      const svg = (fixture.nativeElement as HTMLElement).querySelector('svg');

      expect(svg?.getAttribute('aria-hidden')).toBe('true');
      expect(svg?.querySelectorAll('path, rect, circle').length).toBeGreaterThan(0);
    }
  });
});

describe('ThemeService', () => {
  beforeEach(() => {
    localStorage.clear();
    delete document.documentElement.dataset['theme'];
  });

  it('starts on the table cloth theme and applies it to the page', () => {
    const service = TestBed.inject(ThemeService);
    TestBed.tick();

    expect(service.theme()).toBe('tapis');
    expect(document.documentElement.dataset['theme']).toBe('tapis');
  });

  it('switches theme, remembers it, and restores it next time', () => {
    const service = TestBed.inject(ThemeService);
    service.toggle();
    TestBed.tick();

    expect(document.documentElement.dataset['theme']).toBe('nuit');
    expect(localStorage.getItem('uno.theme')).toBe('nuit');
  });
});
