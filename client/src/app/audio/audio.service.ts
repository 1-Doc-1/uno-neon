import { computed, DestroyRef, inject, InjectionToken, Service, signal } from '@angular/core';
import { DOCUMENT } from '@angular/common';
import { MusicEngine, bpmOf } from './music';
import { type Note, SOUNDS, type SoundId } from './sounds';

const ENABLED_KEY = 'uno.sound.enabled';
const VOLUME_KEY = 'uno.sound.volume';
const MUSIC_VOLUME_KEY = 'uno.music.volume';
const TENSION_KEY = 'uno.music.tension';
/** Le volume des effets d'un joueur qui n'a rien réglé (60 %). */
export const DEFAULT_VOLUME = 0.6;
/** Le volume de la musique d'un joueur qui n'a rien réglé (30 %). */
export const DEFAULT_MUSIC_VOLUME = 0.3;
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
  /** Deux circuits séparés, chacun avec son volume : les effets d'un côté, la musique de l'autre. */
  private effectsBus: GainNode | null = null;
  private musicBus: GainNode | null = null;
  private music: MusicEngine | null = null;

  /** Le joueur veut du son (activé par défaut) ; `false` est le « muet général » : ni effets ni musique. */
  readonly enabled = signal(readEnabled());
  /** Volume des effets, de 0 à 1 (60 % par défaut). */
  readonly volume = signal(readVolume(VOLUME_KEY, DEFAULT_VOLUME));
  /** Volume de la musique, de 0 à 1 (30 % par défaut). */
  readonly musicVolume = signal(readVolume(MUSIC_VOLUME_KEY, DEFAULT_MUSIC_VOLUME));
  /** Le joueur veut que la musique monte en tension à la fin de son tour (activé par défaut). */
  readonly tensionEnabled = signal(read(TENSION_KEY) !== 'false');
  /** La tension demandée (la fin de mon tour approche), avant tout réglage du joueur. */
  private readonly tensionRequested = signal(false);
  /** La tension actuelle de la musique, de 0 (calme) à 1 (tendue) : elle monte et retombe progressivement. */
  readonly tension = signal(0);
  readonly musicBpm = computed(() => bpmOf(this.tension()));
  /** Une interaction a eu lieu : le navigateur laisse maintenant jouer. */
  readonly unlocked = signal(false);

  constructor() {
    const unlock = (): void => this.unlock();
    for (const type of UNLOCKING_EVENTS) {
      this.document.addEventListener(type, unlock, { capture: true });
    }
    inject(DestroyRef).onDestroy(() => {
      this.music?.stop();
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
    this.effectsBus = this.context.createGain();
    this.effectsBus.connect(this.context.destination);
    this.musicBus = this.context.createGain();
    this.musicBus.connect(this.context.destination);
    void this.context.resume?.();
    this.unlocked.set(true);
    this.applyMusic();
  }

  setEnabled(enabled: boolean): void {
    this.enabled.set(enabled);
    write(ENABLED_KEY, String(enabled));
    this.applyMusic();
  }

  /** Règle le volume des effets (0 à 1) et le fait entendre par un petit son, sauf si le son est coupé. */
  setVolume(volume: number): void {
    const clamped = clamp01(volume);
    this.volume.set(clamped);
    write(VOLUME_KEY, String(clamped));
    this.play('play');
  }

  /** Règle le volume de la musique (0 à 1) ; à zéro elle s'arrête tout à fait. */
  setMusicVolume(volume: number): void {
    const clamped = clamp01(volume);
    this.musicVolume.set(clamped);
    write(MUSIC_VOLUME_KEY, String(clamped));
    this.applyMusic();
  }

  setTensionEnabled(enabled: boolean): void {
    this.tensionEnabled.set(enabled);
    write(TENSION_KEY, String(enabled));
  }

  /** La fin de mon tour approche (`true`) ou non : la musique monte en tension, puis revient à la normale. */
  setTension(requested: boolean): void {
    this.tensionRequested.set(requested);
  }

  /** Joue un son : rien tant que l'audio n'est pas débloqué, ni quand il est coupé. */
  play(sound: SoundId): void {
    const { context, effectsBus } = this;
    if (!this.enabled() || !context || !effectsBus) {
      return;
    }
    if (context.state === 'suspended') {
      void context.resume();
    }
    const start = context.currentTime;
    effectsBus.gain.setValueAtTime(this.volume(), start);
    for (const tone of SOUNDS[sound]) {
      this.schedule(context, effectsBus, tone, start + tone.at);
    }
  }

  /** La musique joue si le joueur veut du son, avec un volume, et que l'audio est débloqué ; sinon elle est arrêtée. */
  private applyMusic(): void {
    const { context, musicBus } = this;
    if (!context || !musicBus) {
      return;
    }
    musicBus.gain.setValueAtTime(this.musicVolume(), context.currentTime);
    const audible = this.enabled() && this.musicVolume() > 0;
    if (!audible) {
      this.music?.stop();
      return;
    }
    this.music ??= new MusicEngine(
      () => context.currentTime,
      (note, begin) => this.schedule(context, musicBus, note, begin),
      () => (this.tensionRequested() && this.tensionEnabled() ? 1 : 0),
      (level) => this.tension.set(level),
    );
    this.music.start();
  }

  private schedule(context: AudioContext, destination: AudioNode, tone: Note, begin: number): void {
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

function readVolume(key: string, fallback: number): number {
  const stored = read(key);
  const value = Number(stored);
  return stored !== null && Number.isFinite(value) ? clamp01(value) : fallback;
}

function clamp01(value: number): number {
  return Math.min(1, Math.max(0, value));
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
