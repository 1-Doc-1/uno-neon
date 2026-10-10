import type { Note } from './sounds';

/** Tempo au repos, puis au plus tendu (les 5 dernières secondes du tour). */
export const CALM_BPM = 68;
export const TENSE_BPM = 132;

const LOOKAHEAD_SECONDS = 0.4;
const TICK_MS = 100;
/** Part du chemin vers la cible parcourue à chaque tick : la tension monte et retombe en douceur. */
const TENSION_SMOOTHING = 0.22;
const BEATS_PER_CHORD = 4;

interface Chord {
  readonly bass: number;
  /** Les notes de l'arpège, de la plus grave à la plus aiguë (Hz). */
  readonly arpeggio: readonly number[];
}

// La mineur, la plus douce des boucles : Am – F – C – G
const PROGRESSION: readonly Chord[] = [
  { bass: 110, arpeggio: [220, 261.6, 329.6, 440] },
  { bass: 87.3, arpeggio: [174.6, 220, 261.6, 349.2] },
  { bass: 130.8, arpeggio: [261.6, 329.6, 392, 523.3] },
  { bass: 98, arpeggio: [196, 246.9, 293.7, 392] },
];
const ARPEGGIO_PATTERN = [0, 2, 1, 3, 2, 1, 3, 2] as const;

/** Le tempo (battements par minute) pour une tension de 0 à 1. */
export function bpmOf(tension: number): number {
  return CALM_BPM + (TENSE_BPM - CALM_BPM) * clamp01(tension);
}

/**
 * Les notes d'un battement de la boucle, en secondes depuis le début de ce battement. Pure : la boucle dure
 * 16 battements (quatre accords de quatre). La tension ajoute un pouls grave à chaque battement, un tic aigu au
 * contretemps et de l'intensité ; au repos il ne reste qu'un coussin tenu et un arpège léger.
 */
export function beatNotes(beat: number, tension: number, secondsPerBeat: number): readonly Note[] {
  const level = clamp01(tension);
  const chord = PROGRESSION[Math.floor(beat / BEATS_PER_CHORD) % PROGRESSION.length];
  if (!chord) {
    return [];
  }
  const inChord = beat % BEATS_PER_CHORD;
  const loudness = 1 + level * 0.7;
  const notes: Note[] = [];
  if (inChord === 0) {
    const length = secondsPerBeat * BEATS_PER_CHORD;
    notes.push(
      { at: 0, duration: length, type: 'sine', frequency: chord.bass, gain: 0.16 * loudness },
      {
        at: 0,
        duration: length,
        type: 'sine',
        frequency: (chord.arpeggio[0] ?? chord.bass) * 1.5,
        gain: 0.05 * loudness,
      },
    );
  }
  const step = ARPEGGIO_PATTERN[beat % ARPEGGIO_PATTERN.length] ?? 0;
  notes.push({
    at: 0,
    duration: secondsPerBeat * 1.6,
    type: 'triangle',
    frequency: chord.arpeggio[step] ?? chord.bass,
    gain: 0.07 * loudness,
  });
  if (level > 0.05) {
    notes.push(
      { at: 0, duration: 0.16, type: 'sine', frequency: 62, gain: 0.34 * level },
      {
        at: secondsPerBeat / 2,
        duration: 0.05,
        type: 'square',
        frequency: 1320,
        gain: 0.05 * level,
      },
    );
  }
  return notes;
}

/**
 * Le chef d'orchestre de la musique d'ambiance : tant qu'il tourne, il planifie quelques dixièmes de seconde de notes
 * à l'avance dans le contexte audio (jamais de fichier). La tension demandée est lissée à chaque tick.
 */
export class MusicEngine {
  private timer: ReturnType<typeof setInterval> | null = null;
  private nextBeatAt = 0;
  private beat = 0;
  private level = 0;

  constructor(
    private readonly now: () => number,
    private readonly play: (note: Note, startAt: number) => void,
    private readonly targetTension: () => number,
    private readonly onTension: (level: number) => void,
  ) {}

  get running(): boolean {
    return this.timer !== null;
  }

  start(): void {
    if (this.running) {
      return;
    }
    this.nextBeatAt = this.now() + 0.05;
    this.timer = setInterval(() => this.tick(), TICK_MS);
    this.tick();
  }

  stop(): void {
    if (this.timer !== null) {
      clearInterval(this.timer);
      this.timer = null;
    }
    this.level = 0;
    this.onTension(0);
  }

  private tick(): void {
    const target = clamp01(this.targetTension());
    this.level += (target - this.level) * TENSION_SMOOTHING;
    if (Math.abs(target - this.level) < 0.01) {
      this.level = target;
    }
    this.onTension(this.level);
    while (this.nextBeatAt < this.now() + LOOKAHEAD_SECONDS) {
      const secondsPerBeat = 60 / bpmOf(this.level);
      for (const note of beatNotes(this.beat, this.level, secondsPerBeat)) {
        this.play(note, this.nextBeatAt + note.at);
      }
      this.beat++;
      this.nextBeatAt += secondsPerBeat;
    }
  }
}

function clamp01(value: number): number {
  return Math.min(1, Math.max(0, value));
}
