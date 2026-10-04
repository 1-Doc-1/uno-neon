import { type BrowserContext, expect, test as base } from '@playwright/test';
import { Player } from './player.ts';
import { type Stack, startStack } from './stack.ts';

export interface Lobby {
  readonly stack: Stack;
  /** Alice, l'hôte : elle crée le salon. */
  readonly host: Player;
  /** Bob, l'invité : il rejoint par le lien d'invitation. */
  readonly guest: Player;
}

interface Options {
  /** Graine du serveur : fixe les cartes distribuées (voir `support/seeds.ts`). */
  seed: number;
  /** Délai de grâce avant le forfait d'un joueur déconnecté. */
  reconnectGraceMs: number;
  /** Rythme des cartes piochées, en millisecondes (le serveur le dicte, le client le suit). */
  drawStepMs: number;
  /** Réglage de pioche du salon, fixé explicitement pour ne pas dépendre de la valeur par défaut (ADR 0024). */
  drawAmount: 'one' | 'untilPlayable';
}

interface Fixtures {
  stack: Stack;
  /** Deux navigateurs dans le même salon, pas encore lancé : Alice l'a créé, Bob l'a rejoint par le lien. */
  lobby: Lobby;
}

/**
 * Une pile neuve par test (serveur graine fixe + client construit, sur des ports libres) et un contexte de navigateur
 * par joueur. L'ordre est celui du chercheur de graines : Alice se connecte et crée le salon, puis Bob se connecte et
 * rejoint, ce qui fixe l'ordre dans lequel le serveur consomme son aléa avant la distribution.
 */
export const test = base.extend<Options & Fixtures>({
  seed: [1, { option: true }],
  reconnectGraceMs: [60_000, { option: true }],
  drawStepMs: [40, { option: true }],
  drawAmount: ['one', { option: true }],

  stack: async ({ seed, reconnectGraceMs, drawStepMs }, use) => {
    const stack = await startStack({ seed, reconnectGraceMs, drawStepMs });
    await use(stack);
    await stack.stop();
  },

  lobby: async ({ browser, stack, drawAmount }, use) => {
    const contexts: BrowserContext[] = [];
    const newPlayer = async (name: string): Promise<Player> => {
      const context = await browser.newContext({
        baseURL: stack.url,
        viewport: { width: 1280, height: 720 },
        locale: 'fr-FR',
        reducedMotion: 'reduce',
      });
      contexts.push(context);
      return new Player(await context.newPage(), name);
    };

    const host = await newPlayer('Alice');
    const code = await host.createRoom();
    const drawLabel = drawAmount === 'one' ? '1 carte' : 'Jusqu’à pouvoir jouer';
    const draw = host.page.getByRole('radio', { name: drawLabel, exact: true });
    await draw.click();
    await expect(draw).toHaveAttribute('aria-checked', 'true');
    const guest = await newPlayer('Bob');
    await guest.joinByLink(code);
    await expect(host.page.getByText('Bob', { exact: true })).toBeVisible();

    await use({ stack, host, guest });

    // La trace de chaque contexte est gardée par Playwright (use.trace) : rien d'autre à faire que fermer
    await Promise.all(contexts.map((context) => context.close().catch(() => undefined)));
  },
});

export { expect };
