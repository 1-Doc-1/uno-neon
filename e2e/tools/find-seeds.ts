// Cherche des graines (UNO_TEST_SEED) qui donnent les situations dont les tests E2E ont besoin, en jouant la même
// politique que les tests (`choosePlay`) directement par le protocole.
//   Usage : node tools/find-seeds.ts [premièreGraine] [nombre]
// Les graines retenues sont copiées à la main dans `support/seeds.ts`, avec ce qu'elles garantissent.
import { type ChildProcess, spawn } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import type {
  ClientEvent,
  PlayerView,
} from "../../client/src/app/protocol/generated/protocol.ts";
import { Bot, choosePlay } from "../support/protocol-bot.ts";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const binary =
  process.env["UNO_SERVER_BIN"] ??
  join(
    root,
    "server/build/e2e",
    process.platform === "win32" ? "uno_server.exe" : "uno_server",
  );
const MAX_VERSIONS = 90;
// Le réglage de pioche du salon : les graines des tests existants sont trouvées avec « one » (voir `lobby` dans fixtures.ts).
const drawAmount = process.env["UNO_FIND_DRAW_AMOUNT"] ?? "one";

interface Finding {
  seed: number;
  /** Qui joue en premier : A (hôte) ou B. */
  first: "A" | "B";
  discard: string;
  firstPlayable: number;
  firstHand: string[];
  /** Le premier joueur a un +4 : bluff (il a la couleur active) ou légal ? */
  wildDrawFour: "none" | "bluff" | "legal";
  /** Version de la vue où quelqu'un n'a plus qu'une carte pour la première fois, et qui. */
  oneCard: { version: number; who: "A" | "B"; playable: boolean } | null;
  /** Première fois qu'un joueur doit jouer sans aucune carte jouable (pioche guidée automatique) : version et qui. */
  noPlayable: { version: number; who: "A" | "B" } | null;
  /** Première pioche de 3 cartes ou plus d'un coup (cartes piochées une à une) : version de la vue, qui, combien. */
  multiDraw: {
    version: number;
    who: "A" | "B";
    cards: number;
    handSize: number;
  } | null;
  /** Version de la vue où la partie finit (manche unique), et qui gagne. */
  end: { version: number; winner: "A" | "B" } | null;
}

const label = (card: { color: string | null; rank: string }): string =>
  `${card.rank}${card.color ? `-${card.color}` : ""}`;

/** Une pioche d'au moins 3 cartes, une par événement, parmi des événements tout frais. */
function longestDraw(
  events: readonly ClientEvent[],
): { playerId: string; cards: number } | null {
  let best: { playerId: string; cards: number } | null = null;
  let run = 0;
  let previous = "";
  for (const event of events) {
    const single = event.kind === "cardsDrawn" && event.count === 1;
    run = single && event.playerId === previous ? run + 1 : single ? 1 : 0;
    previous = single ? event.playerId : "";
    if (single && run >= 3) best = { playerId: event.playerId, cards: run };
  }
  return best;
}

async function run(seed: number, port: number): Promise<Finding> {
  const server: ChildProcess = spawn(binary, [], {
    cwd: dirname(binary),
    stdio: "ignore",
    env: {
      ...process.env,
      UNO_PORT: String(port),
      UNO_ALLOWED_ORIGINS: "http://127.0.0.1:1",
      UNO_TEST_SEED: String(seed),
      UNO_LOG_LEVEL: "off",
    },
  });
  try {
    for (let attempt = 0; attempt < 100; attempt++) {
      try {
        if ((await fetch(`http://127.0.0.1:${port}/health`)).ok) break;
      } catch {
        await new Promise((done) => setTimeout(done, 50));
      }
    }
    const url = `ws://127.0.0.1:${port}/ws`;
    const a = new Bot(url, "http://127.0.0.1:1");
    const b = new Bot(url, "http://127.0.0.1:1");
    // Même ordre que dans le navigateur : A se connecte et crée, puis B se connecte et rejoint
    await a.open();
    const created = a.send("room.create", {
      nickname: "Alice",
      settings: { matchLength: "singleRound", drawAmount },
    });
    await created;
    while (!a.roomCode) await new Promise((done) => setTimeout(done, 5));
    await b.open();
    await b.send("room.join", { code: a.roomCode, nickname: "Bob" });
    await b.send("room.setReady", { ready: true });
    const firstViews = Promise.all([a.nextUpdate(), b.nextUpdate()]);
    await a.send("match.start", {});
    await firstViews;

    const start = a.view as PlayerView;
    const first: "A" | "B" = start.currentPlayerId === a.playerId ? "A" : "B";
    const firstBot = first === "A" ? a : b;
    const firstView = firstBot.view as PlayerView;
    const hand = firstView.me.hand;
    const wd4 = hand.some((card) => card.rank === "wildDrawFour");
    const hasActiveColor = hand.some(
      (card) => card.color !== null && card.color === firstView.currentColor,
    );
    const finding: Finding = {
      seed,
      first,
      discard:
        label(firstView.discardTop) + ` (couleur ${firstView.currentColor})`,
      firstPlayable: firstView.me.playableCardIds.length,
      firstHand: hand.map(label),
      wildDrawFour: !wd4 ? "none" : hasActiveColor ? "bluff" : "legal",
      oneCard: null,
      noPlayable: null,
      multiDraw: null,
      end: null,
    };

    // La partie complète, jouée par la politique
    const bots = { A: a, B: b } as const;
    let scanned = 0;
    const lastActed = new Map<string, number>();
    for (let round = 0; round < 4000; round++) {
      const view = a.view as PlayerView;
      if (view.phase === "matchOver" || view.phase === "roundOver") {
        const winnerSeat = view.players.find((seat) => seat.cardCount === 0);
        finding.end = {
          version: view.stateVersion,
          winner: winnerSeat?.playerId === a.playerId ? "A" : "B",
        };
        break;
      }
      if (view.stateVersion > MAX_VERSIONS) break;
      const fresh = a.events.slice(scanned);
      scanned = a.events.length;
      const draw = longestDraw(fresh);
      // Retenue seulement si c'est Alice qui pioche et qu'elle doit ensuite choisir (carte spéciale) : l'écran reste figé
      if (
        !finding.multiDraw &&
        draw?.playerId === a.playerId &&
        view.me.canKeepDrawnCard
      ) {
        finding.multiDraw = {
          version: view.stateVersion,
          who: "A",
          cards: draw.cards,
          handSize: view.me.hand.length,
        };
      }
      for (const [name, bot] of Object.entries(bots)) {
        const mine = bot.view as PlayerView;
        const lone = mine.players.find((seat) => seat.cardCount === 1);
        if (lone && !finding.oneCard) {
          const who = lone.playerId === a.playerId ? "A" : "B";
          finding.oneCard = {
            version: mine.stateVersion,
            who,
            playable: false,
          };
        }
        if (
          finding.oneCard &&
          finding.oneCard.who === name &&
          !finding.oneCard.playable
        ) {
          finding.oneCard.playable =
            mine.currentPlayerId === mine.me.playerId &&
            mine.me.playableCardIds.length > 0;
        }
        if (
          !finding.noPlayable &&
          mine.currentPlayerId === mine.me.playerId &&
          mine.me.playableCardIds.length === 0 &&
          mine.me.canDraw &&
          !mine.me.penaltyResponse &&
          !mine.me.canChooseColor
        ) {
          finding.noPlayable = {
            version: mine.stateVersion,
            who: name as "A" | "B",
          };
        }
        if (lastActed.get(name) === mine.stateVersion) continue;
        const move = choosePlay(mine);
        if (move) {
          lastActed.set(name, mine.stateVersion);
          await bot.send(move.type, move.payload);
        } else if (
          mine.currentPlayerId === mine.me.playerId &&
          mine.me.canDraw &&
          mine.me.playableCardIds.length === 0
        ) {
          lastActed.set(name, mine.stateVersion);
          await bot.send("game.drawCard", {});
        }
      }
      await Promise.race([
        a.nextUpdate(),
        b.nextUpdate(),
        new Promise((done) => setTimeout(done, 300)),
      ]);
    }
    a.close();
    b.close();
    return finding;
  } finally {
    server.kill();
  }
}

const firstSeed = Number(process.argv[2] ?? 1);
const count = Number(process.argv[3] ?? 50);
const parallel = 6;
const queue = Array.from({ length: count }, (_, index) => firstSeed + index);
const findings: Finding[] = [];
await Promise.all(
  Array.from({ length: parallel }, async (_, lane) => {
    for (let seed = queue.shift(); seed !== undefined; seed = queue.shift()) {
      try {
        const finding = await run(seed, 19000 + lane);
        findings.push(finding);
        console.log(JSON.stringify(finding));
      } catch (error) {
        console.error(`graine ${seed} : ${String(error)}`);
      }
    }
  }),
);
