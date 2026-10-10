import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it } from 'vitest';
import { AUDIO_CONTEXT_FACTORY, AudioService } from '../audio/audio.service';
import { SoundControl } from './sound-control';

describe('SoundControl', () => {
  let audio: AudioService;

  function render() {
    const fixture = TestBed.createComponent(SoundControl);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    return {
      fixture,
      host,
      toggle: () => host.querySelector<HTMLButtonElement>('button.toggle'),
      slider: () => host.querySelector<HTMLInputElement>('input[type="range"]'),
    };
  }

  beforeEach(() => {
    localStorage.clear();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => null }],
    });
    audio = TestBed.inject(AudioService);
  });

  it('says the sound is on, at 60 %, by default', () => {
    const { toggle, slider } = render();

    expect(toggle()?.textContent).toContain('Son activé');
    expect(slider()?.getAttribute('aria-label')).toBe('Volume');
    expect(slider()?.value).toBe('60');
  });

  it('switches the sound off and on, and hides the volume while it is off', () => {
    const { fixture, toggle, slider } = render();

    toggle()?.click();
    fixture.detectChanges();
    expect(audio.enabled()).toBe(false);
    expect(toggle()?.textContent).toContain('Son coupé');
    expect(slider()).toBeNull();

    toggle()?.click();
    fixture.detectChanges();
    expect(audio.enabled()).toBe(true);
    expect(slider()).not.toBeNull();
  });

  it('keeps only the icon on a small screen, the text staying there for screen readers', () => {
    const fixture = TestBed.createComponent(SoundControl);
    fixture.componentRef.setInput('compact', true);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;

    expect(host.querySelector('input[type="range"]')).toBeNull();
    expect(host.querySelector('button span.sr-only')?.textContent).toContain('Son activé');
  });

  it('sets the volume of the player from the slider', () => {
    const { slider } = render();
    const input = slider() as HTMLInputElement;

    input.value = '35';
    input.dispatchEvent(new Event('input'));

    expect(audio.volume()).toBeCloseTo(0.35);
  });
});
