import type { SoundId } from '../../../audio/sounds';
import type { EffectSpec } from './effect-plan';

/**
 * Le son d'un effet : un effet, un son (SPEC §13). Pure : le chef d'orchestre le joue au moment où l'effet démarre, donc
 * en phase avec l'animation. `null` : cet effet est muet. Une pioche n'a pas de son ici, il se joue à chaque carte.
 */
export function soundOfEffect(spec: EffectSpec, meId: string | null): SoundId | null {
  switch (spec.kind) {
    case 'play':
      return 'play';
    case 'draw':
      return null;
    case 'bigText':
      return spec.text === '+2' ? 'plusTwo' : 'plusFour';
    case 'plusFive':
      return 'plusFive';
    case 'skip':
      return 'skip';
    case 'reverse':
      return 'reverse';
    case 'wheel':
      return 'color';
    case 'turn':
      return spec.to === meId ? 'myTurn' : null;
    case 'uno':
      return 'uno';
    case 'caught':
    case 'challenge':
      return 'caught';
    case 'spotlight':
      return spec.playerId === meId ? 'win' : 'lose';
  }
}
