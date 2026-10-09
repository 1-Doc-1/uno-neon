import type { Card, PlayerView, SeatView, UnoWindow } from '../protocol/generated/protocol';

export type ScenarioName =
  | 'uno-window'
  | 'two-windows'
  | 'challenge'
  | 'must-declare'
  | 'full-table'
  | 'players-2'
  | 'players-3'
  | 'players-4'
  | 'players-6'
  | 'my-turn-2'
  | 'my-turn-3'
  | 'my-turn-4'
  | 'my-turn-6';

export const SCENARIOS: readonly ScenarioName[] = [
  'uno-window',
  'two-windows',
  'challenge',
  'must-declare',
  'full-table',
  'players-2',
  'players-3',
  'players-4',
  'players-6',
  'my-turn-2',
  'my-turn-3',
  'my-turn-4',
  'my-turn-6',
];

const card = (id: number, color: Card['color'], rank: Card['rank']): Card => ({ id, color, rank });

const HAND: readonly Card[] = [
  card(1, 'red', '7'),
  card(2, 'red', 'skip'),
  card(3, 'blue', '7'),
  card(4, 'green', '2'),
  card(5, 'yellow', 'drawTwo'),
  card(6, null, 'wild'),
  card(7, 'blue', '9'),
  card(8, 'green', 'reverse'),
  card(9, 'red', '0'),
  card(10, null, 'wildDrawFour'),
];

const seat = (
  id: string,
  nickname: string,
  index: number,
  cardCount: number,
  extra: Partial<SeatView> = {},
): SeatView => ({
  playerId: id,
  nickname,
  seat: index,
  cardCount,
  score: 40 * index,
  isConnected: true,
  isBot: false,
  isHost: index === 0,
  hasCalledUno: false,
  isReadyForNextRound: false,
  ...extra,
});

/**
 * Des vues de table écrites à la main pour la page `/dev/table` : on y voit les situations difficiles à provoquer en
 * jouant (fenêtre de contre-UNO ouverte, contestation d'un +4, table pleine). `now` est l'heure locale : les échéances
 * en dérivent, le décalage d'horloge vaut zéro.
 */
export function scenarioView(name: ScenarioName, now: number): PlayerView {
  const me = seat('me', 'Toi', 0, HAND.length);
  const loic = seat('loic', 'Loïc', 1, 1);
  const zoe = seat('zoe', 'Zoé', 2, 6);
  const window = (targetId: string, graceLeftMs: number, openMs: number): UnoWindow => ({
    targetId,
    graceEndsAt: now + graceLeftMs,
    expiresAt: now + openMs,
  });

  const base: PlayerView = {
    stateVersion: 30,
    phase: 'awaitingPlay',
    me: {
      playerId: 'me',
      hand: [...HAND],
      playableCardIds: [1, 2, 9, 6, 10],
      canDraw: true,
      canKeepDrawnCard: false,
      canCallUno: false,
      mustDeclareUno: false,
      canChooseColor: false,
      penaltyResponse: null,
    },
    players: [me, loic, zoe],
    currentPlayerId: 'me',
    direction: 'clockwise',
    currentColor: 'red',
    discardTop: card(50, 'red', '5'),
    drawPileCount: 61,
    pendingDraw: 0,
    turnDeadline: now + 24_000,
    nextRoundDeadline: null,
    actionsOpenAt: now,
    drawStepMs: 1000,
    unoWindows: [],
    round: 2,
    settings: {
      stacking: 'off',
      jumpIn: false,
      sevenZero: false,
      drawAmount: 'untilPlayable',
      wildDrawFourMode: 'officialChallenge',
      turnTimerSeconds: 30,
      matchLength: 'to500',
      maxPlayers: 6,
      drawRule: 'guided',
      declareUnoToWin: false,
    },
    roundResult: null,
    matchWinnerId: null,
  };

  switch (name) {
    case 'uno-window':
      return { ...base, unoWindows: [window('loic', -3_000, 11_000)] };
    case 'two-windows':
      return {
        ...base,
        players: [me, seat('loic', 'Loïc', 1, 1), seat('zoe', 'Zoé', 2, 1)],
        unoWindows: [window('loic', -3_000, 11_000), window('zoe', 1_200, 13_000)],
      };
    case 'challenge':
      return {
        ...base,
        phase: 'awaitingPenaltyResponse',
        currentColor: 'blue',
        discardTop: card(51, null, 'wildDrawFour'),
        pendingDraw: 4,
        me: {
          ...base.me,
          canDraw: false,
          playableCardIds: [],
          penaltyResponse: { amount: 4, canChallenge: true, canStack: false },
        },
      };
    case 'must-declare':
      return {
        ...base,
        players: [seat('me', 'Toi', 0, 1), loic, zoe],
        me: {
          ...base.me,
          hand: [card(1, 'red', '7')],
          playableCardIds: [1],
          canDraw: false,
          canCallUno: true,
          mustDeclareUno: true,
        },
        settings: { ...base.settings, declareUnoToWin: true },
      };
    case 'players-2':
    case 'players-3':
    case 'players-4':
    case 'players-6':
    case 'my-turn-2':
    case 'my-turn-3':
    case 'my-turn-4':
    case 'my-turn-6': {
      // N joueurs autour de la table (moi compris) : N - 1 adversaires ; l'un d'eux joue, ou moi (`my-turn-N`)
      const total = Number(name.slice(-1));
      const mine = name.startsWith('my-turn-');
      const names = ['Loïc', 'Zoé', 'Camille', 'Max', 'Inès', 'Noa'];
      const counts = [4, 7, 2, 11, 5, 9];
      const others = Array.from({ length: total - 1 }, (_, index) =>
        seat(`o${index}`, names[index] ?? 'Joueur', index + 1, counts[index] ?? 5, {
          isConnected: total !== 6 || index !== 3,
        }),
      );
      return {
        ...base,
        players: [me, ...(others as [SeatView, ...SeatView[]])],
        currentPlayerId: mine ? 'me' : others[Math.min(1, others.length - 1)].playerId,
        me: mine ? base.me : { ...base.me, canDraw: false, playableCardIds: [] },
        turnDeadline: now + 17_000,
      };
    }
    case 'full-table':
      return {
        ...base,
        players: [
          me,
          seat('a', 'Camille', 1, 3),
          seat('b', 'Loïc', 2, 7),
          seat('c', 'Zoé', 3, 14),
          seat('d', 'Max', 4, 1, { hasCalledUno: true }),
          seat('e', 'Inès', 5, 5, { isConnected: false }),
          seat('f', 'Noa', 6, 9),
        ],
        currentPlayerId: 'c',
        me: { ...base.me, canDraw: false, playableCardIds: [] },
        drawPileCount: 24,
      };
  }
}

export const SCENARIO_JOURNAL: readonly string[] = ['Zoé pose 5 rouge ▲.', 'Loïc pioche 1 carte.'];
