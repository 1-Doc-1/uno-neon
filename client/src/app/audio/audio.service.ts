import { DestroyRef, inject, InjectionToken, Service, signal } from '@angular/core';
import { DOCUMENT } from '@angular/common';
import { type Note, SOUNDS, type SoundId } from './sounds';

const ENABLED_KEY = 'uno.sound.enabled';
const VOLUME_KEY = 'uno.sound.volume';
/** Le volume d'un joueur qui n'a rien réglé (60 %). */
export const DEFAULT_VOLUME = 0.6;
/** Un grand silence ne coupe pas net : l'enveloppe d'une note ne descend jamais à zéro (impossible en exponentielle). */
const SILENCE = 0.0001;
const ATTACK_SECONDS = 0.008;
/** Les gestes qui comptent pour le navigateur (règle d'autoplay) : le premier débloque l'audio. */
const UNLOCKING_EVENTS = ['pointerdown', 'keydown', 'touchstart'] as const;

/** Fabrique du contexte audio : le navigateur par défaut, un faux dans les tests. `null` : pas d'audio du tout. */
export const AUDIO_CONTEXT_FACTORY = new InjectionToken<() => AudioContext | null>(
  'AUDIO_CONTEXT_FACTORY',
  {
    providedIn: 'root',
    factory: () => () => {
      const scope = globalThis as {
        AudioContext?: typeof AudioContext;
        webkitAudioContext?: typeof AudioContext;
      };
      const Context = scope.AudioContext ?? scope.webkitAudioContext;
      return Context ? new Context() : null;
    },
  },
);

/**
 * Le seul endroit qui fait du bruit (SPEC §13). Il ne démarre qu'après une interaction de l'utilisateur (règle
 * d'autoplay des navigateurs), se tait quand le joueur l'a coupé, et ne joue que ce que l'`AnimationDirector` lui
 * demande : jamais un composant directement. Réglages personnels de chaque joueur, gardés dans `localStorage`.
 */
@Service()
export class AudioService {
  private readonly createContext = inject(AUDIO_CONTEXT_FACTORY);
  private readonly document = inject(DOCUMENT);
  private context: AudioContext | null = null;
  private master: GainNode | null = null;

  /** Le joueur veut du son (activé par défaut). */
  readonly enabled = signal(readEnabled());
  /** Volume de 0 à 1 (60 % par défaut). */
  readonly volume = signal(readVolume());
  /** Une interaction a eu lieu : le navigateur laisse maintenant jouer. */
  readonly unlocked = signal(false);

  constructor() {
    const unlock = (): void => this.unlock();
    for (const type of UNLOCKING_EVENTS) {
      this.document.addEventListener(type, unlock, { capture: true });
    }
    inject(DestroyRef).onDestroy(() => {
      for (const type of UNLOCKING_EVENTS) {
        this.document.removeEventListener(type, unlock, { capture: true });
      }
    });
  }

  /** Crée le contexte audio. Appelé par la première interaction, jamais avant. */
  unlock(): void {
    if (this.context) {
      return;
    }
    try {
      this.context = this.createContext();
    } catch {
      this.context = null;
    }
    if (!this.context) {
      return;
    }
    this.master = this.context.createGain();
    this.master.connect(this.context.destination);
    void this.context.resume?.();
    this.unlocked.set(true);
  }

  setEnabled(enabled: boolean): void {
    this.enabled.set(enabled);
    write(ENABLED_KEY, String(enabled));
  }

  /** Règle le volume (0 à 1) et le fait entendre par un petit son, sauf si le son est coupé. */
  setVolume(volume: number): void {
    const clamped = Math.min(1, Math.max(0, volume));
    this.volume.set(clamped);
    write(VOLUME_KEY, String(clamped));
    this.play('play');
  }

  /** Joue un son : rien tant que l'audio n'est pas débloqué, ni quand il est coupé. */
  play(sound: SoundId): void {
    const { context, master } = this;
    if (!this.enabled() || !context || !master) {
      return;
    }
    if (context.state === 'suspended') {
      void context.resume();
    }
    const start = context.currentTime;
    master.gain.setValueAtTime(this.volume(), start);
    for (const tone of SOUNDS[sound]) {
      this.schedule(context, master, tone, start);
    }
  }

  private schedule(context: AudioContext, destination: AudioNode, tone: Note, start: number): void {
    const begin = start + tone.at;
    const end = begin + tone.duration;
    const oscillator = context.createOscillator();
    const envelope = context.createGain();
    oscillator.type = tone.type;
    oscillator.frequency.setValueAtTime(tone.frequency, begin);
    if (tone.to !== undefined) {
      oscillator.frequency.exponentialRampToValueAtTime(tone.to, end);
    }
    // Une attaque courte puis une chute douce : sans elle, chaque note claque en début et en fin
    envelope.gain.setValueAtTime(SILENCE, begin);
    envelope.gain.linearRampToValueAtTime(tone.gain, begin + ATTACK_SECONDS);
    envelope.gain.exponentialRampToValueAtTime(SILENCE, end);
    oscillator.connect(envelope);
    envelope.connect(destination);
    oscillator.start(begin);
    oscillator.stop(end + 0.02);
  }
}

function readEnabled(): boolean {
  return read(ENABLED_KEY) !== 'false';
}

function readVolume(): number {
  const stored = Number(read(VOLUME_KEY));
  return read(VOLUME_KEY) !== null && Number.isFinite(stored)
    ? Math.min(1, Math.max(0, stored))
    : DEFAULT_VOLUME;
}

function read(key: string): string | null {
  try {
    return localStorage.getItem(key);
  } catch {
    return null;
  }
}

function write(key: string, value: string): void {
  try {
    localStorage.setItem(key, value);
  } catch {
    // Navigation privée, stockage plein : le réglage vaut pour la session, c'est tout
  }
}
