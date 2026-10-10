import { Component, computed, input, output } from '@angular/core';
import type {
  CardMultiplier,
  DrawAmount,
  DrawRule,
  MatchLength,
  PenaltyStacking,
  RoomSettings,
  RoomSettingsPatch,
  TurnTimerSeconds,
} from '../../protocol/generated/protocol';
import { SegmentOption, Segmented } from '../../ui/segmented';
import { deckSize } from './deck-size';

const MATCH_LENGTHS: readonly SegmentOption<MatchLength>[] = [
  { value: 'singleRound', label: 'Manche unique' },
  { value: 'to250', label: '250 points' },
  { value: 'to500', label: '500 points' },
];
const TURN_TIMERS: readonly SegmentOption<TurnTimerSeconds>[] = [
  { value: 0, label: 'Sans' },
  { value: 15, label: '15 s' },
  { value: 30, label: '30 s' },
  { value: 60, label: '60 s' },
];

const DRAW_RULES: readonly SegmentOption<DrawRule>[] = [
  { value: 'guided', label: 'Guidée' },
  { value: 'official', label: 'Officielle' },
];

const DRAW_AMOUNTS: readonly SegmentOption<DrawAmount>[] = [
  { value: 'untilPlayable', label: 'Jusqu’à pouvoir jouer' },
  { value: 'one', label: '1 carte' },
];

const STACKINGS: readonly SegmentOption<PenaltyStacking>[] = [
  { value: 'official', label: 'Sans cumul' },
  { value: 'ladder', label: 'Échelle' },
];

const MULTIPLIERS: readonly SegmentOption<CardMultiplier>[] = [
  { value: 1, label: '×1' },
  { value: 2, label: '×2' },
  { value: 3, label: '×3' },
  { value: 5, label: '×5' },
];

type LastCardRule = 'free' | 'declare';

const LAST_CARD_RULES: readonly SegmentOption<LastCardRule>[] = [
  { value: 'free', label: 'Libre' },
  { value: 'declare', label: 'UNO obligatoire' },
];

const MAX_PLAYERS: readonly SegmentOption<number>[] = [2, 3, 4, 5, 6, 7, 8, 9, 10].map((n) => ({
  value: n,
  label: String(n),
}));

/**
 * Les réglages de partie que l'hôte choisit : ceux du salon et ceux d'une partie contre des bots (SPEC §4, §12.1).
 * Un composant de présentation : il montre `settings` et rend le morceau modifié par `changed`.
 */
@Component({
  selector: 'app-game-settings',
  imports: [Segmented],
  templateUrl: './game-settings.html',
  styleUrl: './game-settings.scss',
})
export class GameSettings {
  readonly settings = input.required<RoomSettings>();
  /** Le nombre de joueurs maximum n'a pas de sens contre des bots : le serveur le fixe. */
  readonly showMaxPlayers = input(true);
  readonly changed = output<RoomSettingsPatch>();

  protected readonly matchLengths = MATCH_LENGTHS;
  protected readonly turnTimers = TURN_TIMERS;
  protected readonly maxPlayers = MAX_PLAYERS;
  protected readonly drawRules = DRAW_RULES;
  protected readonly drawAmounts = DRAW_AMOUNTS;
  protected readonly lastCardRules = LAST_CARD_RULES;
  protected readonly stackings = STACKINGS;
  protected readonly multipliers = MULTIPLIERS;

  protected readonly deckCards = computed(() => deckSize(this.settings()));

  protected lastCardRule(): LastCardRule {
    return this.settings().declareUnoToWin ? 'declare' : 'free';
  }

  protected setLastCardRule(rule: LastCardRule): void {
    this.changed.emit({ declareUnoToWin: rule === 'declare' });
  }
}
