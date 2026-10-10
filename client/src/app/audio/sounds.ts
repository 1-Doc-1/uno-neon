/**
 * Les sons du jeu, tous synthétisés (Web Audio API) : aucun fichier audio, donc aucune question de licence. Un son est
 * une courte liste de notes ; `AudioService` les joue. Un son = un effet de l'animation (SPEC §13).
 */
export type SoundId =
  | 'click'
  | 'intro'
  | 'play'
  | 'draw'
  | 'plusTwo'
  | 'plusFour'
  | 'plusFive'
  | 'skip'
  | 'reverse'
  | 'color'
  | 'uno'
  | 'caught'
  | 'myTurn'
  | 'win'
  | 'lose';

export interface Note {
  /** Départ de la note, en secondes après le début du son. */
  readonly at: number;
  readonly duration: number;
  readonly type: OscillatorType;
  /** Fréquence de départ (Hz) ; `to` la fait glisser jusqu'à la fin de la note. */
  readonly frequency: number;
  readonly to?: number;
  /** Intensité de la note (0 à 1), avant le volume du joueur. */
  readonly gain: number;
}

const note = (
  at: number,
  duration: number,
  type: OscillatorType,
  frequency: number,
  gain: number,
  to?: number,
): Note =>
  to === undefined
    ? { at, duration, type, frequency, gain }
    : { at, duration, type, frequency, gain, to };

/** Une suite de notes égales qui se suivent : arpèges, fanfares. */
const run = (
  type: OscillatorType,
  frequencies: readonly number[],
  step: number,
  duration: number,
  gain: number,
): readonly Note[] =>
  frequencies.map((frequency, index) => note(index * step, duration, type, frequency, gain));

export const SOUNDS: Readonly<Record<SoundId, readonly Note[]>> = {
  // Clic d'interface : un tic très bref et discret
  click: [note(0, 0.035, 'triangle', 900, 0.12, 600)],
  // Ouverture, calée sur la cinématique (2,7 s) : les tuiles tombent, les cartes s'envolent, un accord les réunit
  intro: [
    ...run('triangle', [262, 330, 392, 523], 0.11, 0.14, 0.22),
    note(0.5, 0.9, 'sine', 180, 0.3, 720),
    ...run('sine', [523, 659, 784, 988, 1175, 1319, 1568], 0.09, 0.3, 0.14).map((tone) => ({
      ...tone,
      at: tone.at + 0.55,
    })),
    note(1.5, 1.1, 'triangle', 392, 0.26),
    note(1.5, 1.1, 'triangle', 494, 0.22),
    note(1.5, 1.1, 'triangle', 587, 0.22),
    note(1.5, 1.1, 'sine', 1175, 0.12),
  ],
  // Une carte qu'on pose : un « toc » sourd et un petit claquement
  play: [note(0, 0.09, 'triangle', 240, 0.5, 110), note(0, 0.04, 'square', 1200, 0.12, 600)],
  // Une carte qu'on tire : un souffle qui monte
  draw: [note(0, 0.14, 'sine', 420, 0.3, 760)],
  // +2 : deux coups rauques qui tombent, sous un grondement
  plusTwo: [
    ...run('square', [392, 294], 0.14, 0.16, 0.24),
    note(0, 0.32, 'sawtooth', 110, 0.22, 70),
  ],
  // +4 : trois notes menaçantes qui descendent, un tonnerre grave dessous
  plusFour: [
    ...run('sawtooth', [440, 349, 262], 0.12, 0.16, 0.24),
    note(0, 0.5, 'sawtooth', 90, 0.28, 45),
    note(0.36, 0.3, 'square', 131, 0.2),
  ],
  // +5 : le son doré, un arpège cristallin qui scintille, distinct de tous les autres
  plusFive: [
    ...run('triangle', [784, 988, 1175, 1568], 0.085, 0.18, 0.3),
    ...run('sine', [1568, 1976, 2349, 3136], 0.085, 0.22, 0.12),
    note(0.34, 0.7, 'sine', 3136, 0.16),
    note(0.34, 0.7, 'triangle', 1568, 0.2),
  ],
  // Passe ton tour : un glissando qui tombe, fermé par un « toc » mat
  skip: [note(0, 0.3, 'sine', 760, 0.35, 160), note(0.3, 0.12, 'triangle', 120, 0.4, 80)],
  // Inversion : un grand balayage qui monte, retourne et redescend
  reverse: [
    note(0, 0.22, 'sine', 260, 0.35, 880),
    note(0.22, 0.26, 'sine', 880, 0.35, 260),
    note(0, 0.48, 'triangle', 130, 0.18, 260),
  ],
  // Choix de la couleur : un tourbillon de quatre notes (une par couleur)
  color: run('triangle', [523, 659, 784, 1047], 0.07, 0.1, 0.3),
  // UNO : deux notes claires, comme un « ding ding »
  uno: [note(0, 0.2, 'sine', 880, 0.4), note(0.14, 0.3, 'sine', 1319, 0.4)],
  // Contre-UNO : un buzzer qui claque, puis une chute
  caught: [
    note(0, 0.24, 'sawtooth', 196, 0.32),
    note(0.16, 0.36, 'sawtooth', 147, 0.32),
    note(0.3, 0.3, 'square', 98, 0.2, 55),
  ],
  // À toi de jouer : une cloche discrète
  myTurn: [note(0, 0.16, 'sine', 659, 0.25), note(0.1, 0.24, 'sine', 880, 0.25)],
  // Victoire : une fanfare qui monte et se pose
  win: [
    ...run('triangle', [392, 494, 587], 0.13, 0.16, 0.35),
    note(0.39, 0.55, 'triangle', 784, 0.4),
  ],
  // Défaite : quatre notes qui s'effondrent
  lose: run('sine', [392, 330, 262, 196], 0.2, 0.26, 0.35),
};
