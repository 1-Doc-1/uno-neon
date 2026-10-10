// Une partie complète à deux navigateurs par une adresse publique (le tunnel de scripts/play-online.ps1), au rythme
// réel du serveur. Mesure aussi la taille des messages WebSocket.
//   node tools/tunnel-game.ts https://xxxx.trycloudflare.com
import { chromium, expect, type Page } from '@playwright/test';
import { Player } from '../support/player.ts';

const url = process.argv[2];
if (!url) {
  throw new Error('Usage : node tools/tunnel-game.ts <adresse du tunnel>');
}

interface Frames {
  sent: number[];
  received: number[];
}

function measure(page: Page, frames: Frames): void {
  page.on('websocket', (socket) => {
    socket.on('framesent', (frame) => frames.sent.push(Buffer.byteLength(String(frame.payload))));
    socket.on('framereceived', (frame) =>
      frames.received.push(Buffer.byteLength(String(frame.payload))),
    );
  });
}

const average = (sizes: number[]): number =>
  sizes.length === 0 ? 0 : Math.round(sizes.reduce((sum, size) => sum + size, 0) / sizes.length);

const browser = await chromium.launch();
const frames: Frames = { sent: [], received: [] };
const consoleProblems: string[] = [];
try {
  const contexts = await Promise.all(
    [1, 2].map(() =>
      browser.newContext({ baseURL: url, viewport: { width: 1280, height: 720 }, locale: 'fr-FR' }),
    ),
  );
  const [alice, bob] = await Promise.all(
    contexts.map(async (context, index) => {
      const player = new Player(await context.newPage(), index === 0 ? 'Alice' : 'Bob');
      measure(player.page, frames);
      player.page.on('console', (message) => {
        if (message.type() === 'error') {
          consoleProblems.push(`${player.name}: ${message.text()}`);
        }
      });
      return player;
    }),
  );
  if (!alice || !bob) {
    throw new Error('contextes absents');
  }

  const code = await alice.createRoom();
  await bob.joinByLink(code);
  const single = alice.page.getByRole('radio', { name: 'Manche unique' });
  await single.click();
  await expect(single).toHaveAttribute('aria-checked', 'true');
  await bob.page.getByRole('button', { name: 'Prêt' }).click();
  const start = alice.page.getByRole('button', { name: 'Lancer la partie' });
  await expect(start).toHaveAttribute('aria-disabled', 'false');
  await start.click();
  for (const player of [alice, bob]) {
    await expect(player.cards).toHaveCount(7);
  }

  const startedAt = Date.now();
  const finished = (): Promise<boolean> =>
    alice.page.getByRole('heading', { name: /gagne la manche|remporte la partie/ }).isVisible();
  // Au rythme réel du serveur (1,5 s après chaque action) : on sonde sans la limite de 5 s de playUntil
  while (!(await finished())) {
    const acted = [await alice.actNow(), await bob.actNow()];
    if (!acted.some(Boolean)) {
      await new Promise((resolve) => setTimeout(resolve, 400));
    }
  }
  const seconds = Math.round((Date.now() - startedAt) / 1000);
  console.log(
    JSON.stringify(
      {
        partieTerminee: true,
        secondes: seconds,
        messagesEnvoyes: frames.sent.length,
        messagesRecus: frames.received.length,
        tailleMoyenneEnvoyeeOctets: average(frames.sent),
        tailleMoyenneRecueOctets: average(frames.received),
        plusGrosMessageRecuOctets: Math.max(0, ...frames.received),
        erreursConsole: consoleProblems,
      },
      null,
      2,
    ),
  );
} finally {
  await browser.close();
}
