import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { AUDIO_CONTEXT_FACTORY, AudioService, DEFAULT_VOLUME } from './audio.service';
import { SOUNDS, type SoundId } from './sounds';

/** Un faux AudioContext : il note les oscillateurs créés, avec leur forme et leurs fréquences. */
class FakeAudioContext {
  currentTime = 10;
  state: AudioContextState = 'running';
  readonly destination = {};
  readonly oscillators: { type: string; frequencies: number[]; start: number; stop: number }[] = [];
  readonly masterGains: number[] = [];
  resumed = 0;

  resume(): Promise<void> {
    this.resumed++;
    this.state = 'running';
    return Promise.resolve();
  }

  private gainsCreated = 0;

  createGain() {
    const connected: unknown[] = [];
    const masterGains = this.masterGains;
    // Le premier gain créé est le volume général, les suivants les enveloppes des notes
    const isMaster = ++this.gainsCreated === 1;
    return {
      gain: {
        value: 1,
        setValueAtTime: (value: number) => (isMaster ? masterGains.push(value) : undefined),
        linearRampToValueAtTime: () => undefined,
        exponentialRampToValueAtTime: () => undefined,
      },
      connect: (node: unknown) => connected.push(node),
    };
  }

  createOscillator() {
    const record = { type: '', frequencies: [] as number[], start: -1, stop: -1 };
    this.oscillators.push(record);
    return {
      set type(value: string) {
        record.type = value;
      },
      frequency: {
        setValueAtTime: (value: number) => record.frequencies.push(value),
        exponentialRampToValueAtTime: (value: number) => record.frequencies.push(value),
      },
      connect: () => undefined,
      start: (at: number) => (record.start = at),
      stop: (at: number) => (record.stop = at),
    };
  }
}

describe('AudioService', () => {
  let context: FakeAudioContext;
  let created: number;
  let audio: AudioService;

  beforeEach(() => {
    localStorage.clear();
    context = new FakeAudioContext();
    created = 0;
    TestBed.configureTestingModule({
      providers: [
        {
          provide: AUDIO_CONTEXT_FACTORY,
          useValue: () => {
            created++;
            return context as unknown as AudioContext;
          },
        },
      ],
    });
    audio = TestBed.inject(AudioService);
  });

  const interact = (type = 'pointerdown') => document.dispatchEvent(new Event(type));
  const firstFrequencies = () => context.oscillators.map((oscillator) => oscillator.frequencies[0]);

  it('is on at 60 % by default', () => {
    expect(audio.enabled()).toBe(true);
    expect(audio.volume()).toBe(DEFAULT_VOLUME);
    expect(DEFAULT_VOLUME).toBe(0.6);
  });

  it('plays nothing before the first interaction, and does not even create the audio context', () => {
    audio.play('plusFive');

    expect(created).toBe(0);
    expect(context.oscillators).toHaveLength(0);
    expect(audio.unlocked()).toBe(false);
  });

  it.each(['pointerdown', 'keydown', 'touchstart'])('is unlocked by the first %s, once', (type) => {
    interact(type);
    interact(type);

    expect(created).toBe(1);
    expect(audio.unlocked()).toBe(true);
  });

  it('plays the notes of a sound, at the volume of the player, once unlocked', () => {
    interact();
    audio.setVolume(0.3);
    context.oscillators.length = 0;
    context.masterGains.length = 0;

    audio.play('uno');

    expect(context.oscillators).toHaveLength(SOUNDS.uno.length);
    expect(firstFrequencies()).toEqual(SOUNDS.uno.map((note) => note.frequency));
    expect(context.masterGains).toEqual([0.3]);
    // Chaque note part de l'instant présent de l'horloge audio, décalée de son retard
    expect(context.oscillators.map((oscillator) => oscillator.start)).toEqual(
      SOUNDS.uno.map((note) => context.currentTime + note.at),
    );
  });

  it('makes every sound audibly different: no two sounds start with the same notes', () => {
    interact();
    const signatures = new Set<string>();
    for (const id of Object.keys(SOUNDS) as SoundId[]) {
      context.oscillators.length = 0;
      audio.play(id);
      expect(context.oscillators.length).toBeGreaterThan(0);
      signatures.add(JSON.stringify(context.oscillators.map((o) => [o.type, o.frequencies])));
    }

    expect(signatures.size).toBe(Object.keys(SOUNDS).length);
  });

  it('gives the golden +5 its own sparkle: high, crystalline notes no other sound has', () => {
    interact();
    context.oscillators.length = 0;
    audio.play('plusFive');
    const highest = Math.max(...context.oscillators.map((o) => o.frequencies[0]));
    const others = (Object.keys(SOUNDS) as SoundId[])
      .filter((id) => id !== 'plusFive')
      .flatMap((id) => SOUNDS[id].map((note) => note.frequency));

    expect(highest).toBeGreaterThan(Math.max(...others));
  });

  it('plays nothing when the sound is off, and remembers the choice', () => {
    interact();
    audio.setEnabled(false);
    context.oscillators.length = 0;

    audio.play('win');

    expect(context.oscillators).toHaveLength(0);
    expect(localStorage.getItem('uno.sound.enabled')).toBe('false');
  });

  it('plays again once the sound is back on', () => {
    interact();
    audio.setEnabled(false);
    audio.setEnabled(true);

    audio.play('draw');

    expect(context.oscillators.length).toBeGreaterThan(0);
  });

  it('keeps the volume between 0 and 1 and remembers it', () => {
    audio.setVolume(3);
    expect(audio.volume()).toBe(1);
    audio.setVolume(-1);
    expect(audio.volume()).toBe(0);
    audio.setVolume(0.45);
    expect(localStorage.getItem('uno.sound.volume')).toBe('0.45');
  });

  it('lets the player hear the new volume with a small sound, but not when the sound is off', () => {
    interact();
    context.oscillators.length = 0;
    audio.setVolume(0.8);
    expect(context.oscillators.length).toBeGreaterThan(0);

    audio.setEnabled(false);
    context.oscillators.length = 0;
    audio.setVolume(0.9);
    expect(context.oscillators).toHaveLength(0);
  });

  it('wakes a context the browser suspended', () => {
    interact();
    context.state = 'suspended';

    audio.play('play');

    expect(context.resumed).toBeGreaterThan(1);
  });

  it('reads the choices of the player from the previous visits', () => {
    localStorage.setItem('uno.sound.enabled', 'false');
    localStorage.setItem('uno.sound.volume', '0.2');
    TestBed.resetTestingModule();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => null }],
    });
    const fresh = TestBed.inject(AudioService);

    expect(fresh.enabled()).toBe(false);
    expect(fresh.volume()).toBe(0.2);
  });

  it('survives a browser without any Web Audio', () => {
    TestBed.resetTestingModule();
    TestBed.configureTestingModule({
      providers: [{ provide: AUDIO_CONTEXT_FACTORY, useValue: () => null }],
    });
    const mute = TestBed.inject(AudioService);

    mute.unlock();
    expect(() => mute.play('win')).not.toThrow();
    expect(mute.unlocked()).toBe(false);
  });

  it('survives a storage that refuses to be read or written', () => {
    const broken = vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
      throw new Error('quota');
    });

    expect(() => audio.setEnabled(false)).not.toThrow();
    expect(audio.enabled()).toBe(false);
    broken.mockRestore();
  });
});
