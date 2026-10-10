import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  AUDIO_CONTEXT_FACTORY,
  AudioService,
  DEFAULT_MUSIC_VOLUME,
  DEFAULT_VOLUME,
} from './audio.service';
import { beatNotes, bpmOf, CALM_BPM, TENSE_BPM } from './music';

/** Un faux AudioContext : il note les oscillateurs, et le volume réglé sur chacun des deux circuits (effets, musique). */
class FakeAudioContext {
  currentTime = 10;
  state: AudioContextState = 'running';
  readonly destination = {};
  readonly oscillators: { start: number; gain: number }[] = [];
  readonly busGains: number[][] = [[], []];
  private gains = 0;

  resume(): Promise<void> {
    return Promise.resolve();
  }

  createGain() {
    const index = this.gains++;
    const values = this.busGains[index];
    return {
      gain: {
        setValueAtTime: (value: number) => values?.push(value),
        linearRampToValueAtTime: () => undefined,
        exponentialRampToValueAtTime: () => undefined,
      },
      connect: () => undefined,
    };
  }

  createOscillator() {
    const record = { start: -1, gain: 0 };
    this.oscillators.push(record);
    return {
      type: '',
      frequency: { setValueAtTime: () => undefined, exponentialRampToValueAtTime: () => undefined },
      connect: () => undefined,
      start: (at: number) => (record.start = at),
      stop: () => undefined,
    };
  }
}

describe('the music', () => {
  it('slows down when calm and speeds up with tension', () => {
    expect(bpmOf(0)).toBe(CALM_BPM);
    expect(bpmOf(1)).toBe(TENSE_BPM);
    expect(bpmOf(0.5)).toBeGreaterThan(CALM_BPM);
    expect(bpmOf(0.5)).toBeLessThan(TENSE_BPM);
    expect(bpmOf(9)).toBe(TENSE_BPM);
  });

  it('adds a pulse and more intensity with tension, and nothing but the pad and arpeggio when calm', () => {
    const calm = beatNotes(1, 0, 0.9);
    const tense = beatNotes(1, 1, 0.45);

    expect(calm).toHaveLength(1);
    expect(tense.length).toBeGreaterThan(calm.length);
    expect(Math.max(...tense.map((note) => note.gain))).toBeGreaterThan(
      Math.max(...calm.map((note) => note.gain)),
    );
  });

  it('loops over sixteen beats', () => {
    expect(beatNotes(3, 0, 1).map((n) => n.frequency)).toEqual(
      beatNotes(19, 0, 1).map((n) => n.frequency),
    );
  });
});

describe('AudioService music and tension', () => {
  let context: FakeAudioContext;
  let audio: AudioService;
  const interact = () => document.dispatchEvent(new Event('pointerdown'));
  /** Fait avancer l'horloge audio et la boucle de planification de `seconds` secondes. */
  const elapse = (seconds: number) => {
    for (let elapsed = 0; elapsed < seconds; elapsed += 0.1) {
      context.currentTime += 0.1;
      vi.advanceTimersByTime(100);
    }
  };

  beforeEach(() => {
    localStorage.clear();
    vi.useFakeTimers();
    context = new FakeAudioContext();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => context }],
    });
    audio = TestBed.inject(AudioService);
  });

  afterEach(() => vi.useRealTimers());

  it('has effects at 60 %, music at 30 % and the end-of-turn tension on by default', () => {
    expect(audio.volume()).toBe(DEFAULT_VOLUME);
    expect(audio.musicVolume()).toBe(DEFAULT_MUSIC_VOLUME);
    expect(DEFAULT_MUSIC_VOLUME).toBe(0.3);
    expect(audio.tensionEnabled()).toBe(true);
  });

  it('plays no music before the first interaction', () => {
    elapse(2);

    expect(context.oscillators).toHaveLength(0);
  });

  it('starts the music at the first interaction, on its own circuit at its own volume', () => {
    interact();
    elapse(1);

    expect(context.oscillators.length).toBeGreaterThan(0);
    expect(context.busGains[1]).toContain(0.3);
  });

  it('keeps the two volumes separate, each remembered on its own', () => {
    interact();
    audio.setMusicVolume(0.8);
    audio.setVolume(0.1);

    expect(audio.musicVolume()).toBe(0.8);
    expect(audio.volume()).toBe(0.1);
    expect(context.busGains[1]?.at(-1)).toBe(0.8);
    expect(context.busGains[0]?.at(-1)).toBe(0.1);
    expect(localStorage.getItem('uno.music.volume')).toBe('0.8');
    expect(localStorage.getItem('uno.sound.volume')).toBe('0.1');
  });

  it('stops the music at volume zero or when everything is muted, and resumes after', () => {
    interact();
    elapse(1);
    audio.setMusicVolume(0);
    context.oscillators.length = 0;
    elapse(2);
    expect(context.oscillators).toHaveLength(0);

    audio.setMusicVolume(0.3);
    audio.setEnabled(false);
    context.oscillators.length = 0;
    elapse(2);
    expect(context.oscillators).toHaveLength(0);

    audio.setEnabled(true);
    elapse(1);
    expect(context.oscillators.length).toBeGreaterThan(0);
  });

  it('builds tension in the last seconds, then comes back to normal', () => {
    interact();
    elapse(1);
    expect(audio.musicBpm()).toBe(CALM_BPM);

    audio.setTension(true);
    elapse(3);
    expect(audio.tension()).toBeGreaterThan(0.9);
    expect(audio.musicBpm()).toBeGreaterThan(TENSE_BPM - 5);

    audio.setTension(false);
    elapse(3);
    expect(audio.tension()).toBe(0);
    expect(audio.musicBpm()).toBe(CALM_BPM);
  });

  it('keeps the music calm when the player turned the tension off', () => {
    interact();
    audio.setTensionEnabled(false);
    audio.setTension(true);
    elapse(3);

    expect(audio.tension()).toBe(0);
    expect(localStorage.getItem('uno.music.tension')).toBe('false');
  });

  it('stops the tension when the music is muted', () => {
    interact();
    audio.setTension(true);
    elapse(3);
    audio.setEnabled(false);

    expect(audio.tension()).toBe(0);
  });
});
