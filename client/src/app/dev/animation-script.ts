import type {
  Card,
  ClientEvent,
  Color,
  Direction,
  PlayerView,
  Rank,
} from '../protocol/generated/protocol';
import { scenarioView } from './fixtures';

/** Ce que le scénario de démonstration fait varier d'une étape à l'autre ; tout le reste vient du scénario « mon tour ». */
interface DemoState {
  hand: Card[];
  counts: Record<string, number>;
  discardTop: Card;
  color: Color;
  direction: Direction;
  current: string;
  pending: number;
}

export interface DemoFrame {
  readonly caption: string;
  readonly events: readonly ClientEvent[];
  readonly view: PlayerView;
}

const ME = 'me';
const LOIC = 'o0';
const ZOE = 'o1';

const card = (id: number, color: Color | null, rank: Rank): Card => ({ id, color, rank });
const played = (playerId: string, played: Card, chosenColor: Color | null = null): ClientEvent => ({
  kind: 'cardPlayed',
  playerId,
  card: played,
  chosenColor,
  isJumpIn: false,
});
const turn = (playerId: string): ClientEvent => ({ kind: 'turnChanged', playerId });
const drew = (playerId: string, count: number, cards?: Card[]): ClientEvent => ({
  kind: 'cardsDrawn',
  playerId,
  count,
  ...(cards ? { cards } : {}),
});

interface Step {
  readonly caption: string;
  readonly events: readonly ClientEvent[];
  readonly apply: (state: DemoState) => void;
}

const initialState = (): DemoState => ({
  hand: [
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
  ],
  counts: { [LOIC]: 4, [ZOE]: 7 },
  discardTop: card(50, 'red', '5'),
  color: 'red',
  direction: 'clockwise',
  current: LOIC,
  pending: 0,
});

const dealt = [
  card(11, 'green', '5'),
  card(12, 'blue', '3'),
  card(13, 'yellow', '1'),
  card(14, 'red', '4'),
];

/** La séquence scriptée : tous les effets de jeu, dans un ordre qui raconte une petite partie. */
const STEPS: readonly Step[] = [
  {
    caption: 'Loïc pose un 8 rouge — la carte vole de son siège, face cachée puis retournée',
    events: [played(LOIC, card(60, 'red', '8')), turn(ZOE)],
    apply: (s) => {
      s.counts[LOIC] -= 1;
      s.discardTop = card(60, 'red', '8');
      s.current = ZOE;
    },
  },
  {
    caption:
      'Zoé pose un +2 : « +2 » en grand, mes deux cartes arrivent, mon siège tremble, je passe',
    events: [
      played(ZOE, card(61, 'red', 'drawTwo')),
      drew(ME, 2, [dealt[0], dealt[1]]),
      { kind: 'playerSkipped', playerId: ME },
      turn(LOIC),
    ],
    apply: (s) => {
      s.counts[ZOE] -= 1;
      s.discardTop = card(61, 'red', 'drawTwo');
      s.hand.push(dealt[0], dealt[1]);
      s.current = LOIC;
    },
  },
  {
    caption: 'Loïc pose un Passe-tour : le symbole « interdit » s’imprime sur Zoé',
    events: [
      played(LOIC, card(62, 'red', 'skip')),
      { kind: 'playerSkipped', playerId: ZOE },
      turn(ME),
    ],
    apply: (s) => {
      s.counts[LOIC] -= 1;
      s.discardTop = card(62, 'red', 'skip');
      s.current = ME;
    },
  },
  {
    caption: 'Je pose un Joker bleu : la roue des couleurs, puis le liseré devient bleu',
    events: [
      played(ME, card(6, null, 'wild'), 'blue'),
      { kind: 'colorChosen', playerId: ME, color: 'blue' },
      turn(LOIC),
    ],
    apply: (s) => {
      s.hand = s.hand.filter((c) => c.id !== 6);
      s.discardTop = card(6, null, 'wild');
      s.color = 'blue';
      s.current = LOIC;
    },
  },
  {
    caption:
      'Loïc pose une Inversion : une grande flèche circulaire fait un tour et se retourne, la lueur repart à l’envers',
    events: [
      played(LOIC, card(63, 'blue', 'reverse')),
      { kind: 'directionChanged', direction: 'counterClockwise' },
      turn(ME),
    ],
    apply: (s) => {
      s.counts[LOIC] -= 1;
      s.discardTop = card(63, 'blue', 'reverse');
      s.direction = 'counterClockwise';
      s.current = ME;
    },
  },
  {
    caption: 'Loïc n’a plus qu’une carte : « UNO ! »',
    events: [{ kind: 'unoCalled', playerId: LOIC }],
    apply: () => undefined,
  },
  {
    caption: 'Je pose un +4 vert : « +4 » en grand, puis la roue des couleurs',
    events: [
      played(ME, card(10, null, 'wildDrawFour'), 'green'),
      { kind: 'colorChosen', playerId: ME, color: 'green' },
      turn(ZOE),
    ],
    apply: (s) => {
      s.hand = s.hand.filter((c) => c.id !== 10);
      s.discardTop = card(10, null, 'wildDrawFour');
      s.color = 'green';
      s.pending = 4;
      s.current = ZOE;
    },
  },
  {
    caption: 'Zoé me conteste, c’était un bluff : verdict, puis mes 4 cartes de pénalité',
    events: [
      {
        kind: 'challengeResolved',
        challengerId: ZOE,
        challengedId: ME,
        wasBluff: true,
        penalizedPlayerId: ME,
        penaltyAmount: 4,
      },
      drew(ME, 4, [dealt[2], dealt[3], card(15, 'blue', '6'), card(16, 'green', '8')]),
      turn(LOIC),
    ],
    apply: (s) => {
      s.hand.push(dealt[2], dealt[3], card(15, 'blue', '6'), card(16, 'green', '8'));
      s.pending = 0;
      s.current = LOIC;
    },
  },
  {
    caption:
      'Je contre Zoé qui a oublié d’annoncer UNO : tampon « Contre-UNO ! », puis ses 2 cartes',
    events: [{ kind: 'unoCaught', catcherId: ME, targetId: ZOE, penaltyAmount: 2 }, drew(ZOE, 2)],
    apply: (s) => {
      s.counts[ZOE] += 2;
    },
  },
  {
    caption:
      'Rien à jouer : je pioche jusqu’à pouvoir jouer, une carte par seconde (rythme du serveur), mon tas et le compteur de Loïc et Zoé grandissent à chaque arrivée',
    events: [
      drew(ME, 1, [card(17, 'yellow', '8')]),
      drew(ME, 1, [card(18, 'blue', '5')]),
      drew(ME, 1, [card(19, 'green', '9')]),
      drew(ME, 1, [card(20, 'green', '3')]),
      drew(ME, 1, [card(21, 'red', '6')]),
      drew(ME, 1, [card(22, 'yellow', '1')]),
      turn(LOIC),
    ],
    apply: (s) => {
      s.hand.push(
        card(17, 'yellow', '8'),
        card(18, 'blue', '5'),
        card(19, 'green', '9'),
        card(20, 'green', '3'),
        card(21, 'red', '6'),
        card(22, 'yellow', '1'),
      );
      s.current = LOIC;
    },
  },
  {
    caption: 'Loïc pioche une carte (face cachée vers son siège)',
    events: [drew(LOIC, 1), turn(ZOE)],
    apply: (s) => {
      s.counts[LOIC] += 1;
      s.current = ZOE;
    },
  },
  {
    caption: 'Loïc gagne la manche : le projecteur se braque sur lui',
    events: [{ kind: 'roundEnded', winnerId: LOIC, points: 120 }],
    apply: () => undefined,
  },
];

/** La mise en scène : une première vue complète, puis une vue et ses événements par étape. */
export function buildDemoFrames(now: number): readonly DemoFrame[] {
  const state = initialState();
  let version = 100;

  const snapshot = (events: readonly ClientEvent[], caption: string): DemoFrame => {
    const base = scenarioView('my-turn-3', now);
    const view: PlayerView = {
      ...base,
      stateVersion: version++,
      players: base.players.map((seat) => ({
        ...seat,
        cardCount:
          seat.playerId === ME
            ? state.hand.length
            : (state.counts[seat.playerId] ?? seat.cardCount),
      })) as PlayerView['players'],
      currentPlayerId: state.current,
      direction: state.direction,
      currentColor: state.color,
      discardTop: state.discardTop,
      pendingDraw: state.pending,
      turnDeadline: null,
      me: {
        ...base.me,
        hand: [...state.hand],
        playableCardIds: state.current === ME ? state.hand.map((c) => c.id) : [],
        canDraw: false,
      },
    };
    return { caption, events, view };
  };

  const frames: DemoFrame[] = [snapshot([], 'Début de la séquence')];
  for (const step of STEPS) {
    step.apply(state);
    frames.push(snapshot(step.events, step.caption));
  }
  return frames;
}
