import type { ClientEvent, Card } from '../../../protocol/generated/protocol';
import { EffectSpec, INITIAL_PLAN_STATE, planEffects, timingOf } from './effect-plan';

/**
 * Le budget que le serveur accorde à la présentation d'une action (ADR 0027, `Timeouts` de `application.hpp`) : personne
 * ne joue avant. Ces chiffres sont ceux du serveur ; le test de l'application (`application_presentation_test.cpp`,
 * « Every special card leaves the table the time to see its effect ») verrouille ses propres sommes. Si l'un des deux
 * change sans l'autre, un des deux tests casse.
 */
const SERVER = { playStep: 1100, effectStep: 1500, drawStep: 1000 } as const;

/**
 * Fin de la dernière animation, à vitesse normale, quand les effets d'un lot s'enchaînent comme dans `AnimationDirector`.
 * L'éclat de tour (0,5 s) n'est pas compté : il accompagne l'ouverture du tour du joueur, il ne cache rien qu'il faille
 * avoir vu avant de jouer.
 */
function presentationEndMs(specs: readonly EffectSpec[]): number {
  let start = 0;
  let end = 0;
  for (const spec of specs.filter((candidate) => candidate.kind !== 'turn')) {
    const { stepMs, visibleMs } = timingOf(spec, false, SERVER.drawStep);
    end = Math.max(end, start + visibleMs);
    start += stepMs;
  }
  return Math.max(end, start);
}

const card = (rank: Card['rank'], color: Card['color']): Card => ({ id: 1, rank, color });

function presentation(events: readonly ClientEvent[]): number {
  return presentationEndMs(planEffects(events, INITIAL_PLAN_STATE).specs);
}

const played = (
  rank: Card['rank'],
  color: Card['color'],
  chosenColor: Card['color'] = null,
): ClientEvent => ({
  kind: 'cardPlayed',
  playerId: 'a',
  card: card(rank, color),
  chosenColor,
  isJumpIn: false,
});

describe('the time the server leaves to present each special card', () => {
  it('covers a Skip: the card, then the skipped player', () => {
    const ms = presentation([
      played('skip', 'red'),
      { kind: 'playerSkipped', playerId: 'b' },
      { kind: 'turnChanged', playerId: 'c' },
    ]);

    expect(ms).toBeLessThanOrEqual(SERVER.playStep + SERVER.effectStep);
  });

  it('covers a Reverse: the card, then the big arrow', () => {
    const ms = presentation([
      played('reverse', 'red'),
      { kind: 'directionChanged', direction: 'counterClockwise' },
      { kind: 'turnChanged', playerId: 'c' },
    ]);

    expect(ms).toBeLessThanOrEqual(SERVER.playStep + SERVER.effectStep);
  });

  it('covers a Draw Two: the card, the "+2", the skipped victim and the two cards', () => {
    const ms = presentation([
      played('drawTwo', 'red'),
      { kind: 'cardsDrawn', playerId: 'b', count: 1 },
      { kind: 'cardsDrawn', playerId: 'b', count: 1 },
      { kind: 'playerSkipped', playerId: 'b' },
      { kind: 'turnChanged', playerId: 'c' },
    ]);

    expect(ms).toBeLessThanOrEqual(SERVER.playStep + 2 * SERVER.effectStep + 2 * SERVER.drawStep);
  });

  it('covers a Wild: the card, then the colour wheel', () => {
    const ms = presentation([
      played('wild', null, 'blue'),
      { kind: 'colorChosen', playerId: 'a', color: 'blue' },
      { kind: 'turnChanged', playerId: 'b' },
    ]);

    expect(ms).toBeLessThanOrEqual(SERVER.playStep + SERVER.effectStep);
  });

  it('covers a Wild Draw Four: the card, the "+4" and the colour wheel', () => {
    const ms = presentation([
      played('wildDrawFour', null, 'blue'),
      { kind: 'colorChosen', playerId: 'a', color: 'blue' },
      { kind: 'turnChanged', playerId: 'b' },
    ]);

    expect(ms).toBeLessThanOrEqual(SERVER.playStep + 2 * SERVER.effectStep);
  });
});
