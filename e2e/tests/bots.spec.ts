import { expect, test } from '../support/fixtures.ts';
import { Player, playUntil } from '../support/player.ts';

// Les bots (ADR 0030) : une partie contre des bots sans salon d'attente, et un bot ajouté au salon par l'hôte.
// Le rythme des bots suit `paceMs` (40 ms par défaut) : ils réfléchissent aussi vite que le jeu se présente.

test.describe('Bots', () => {
  test.use({ seed: 5 });

  async function newAlice(browser: import('@playwright/test').Browser, url: string) {
    const context = await browser.newContext({
      baseURL: url,
      viewport: { width: 1280, height: 720 },
      locale: 'fr-FR',
      reducedMotion: 'reduce',
    });
    return { context, alice: new Player(await context.newPage(), 'Alice') };
  }

  test('une partie contre 2 bots, sans salon d’attente, jusqu’à la fin de la manche', async ({
    browser,
    stack,
  }) => {
    test.setTimeout(90_000);
    const { context, alice } = await newAlice(browser, stack.url);
    const { page } = alice;

    await page.goto('/');
    await page.getByLabel('Ton pseudo').fill('Alice');
    await page.getByRole('button', { name: /Jouer contre des bots/ }).click();

    // L'écran « Jouer contre des bots » : nombre, niveau et réglages de partie du salon
    await expect(page.getByRole('heading', { name: 'Jouer contre des bots' })).toBeVisible();
    await page
      .getByRole('radiogroup', { name: 'Nombre de bots' })
      .getByRole('radio', { name: '2' })
      .click();
    await page
      .getByRole('radiogroup', { name: 'Niveau des bots' })
      .getByRole('radio', { name: 'Normal' })
      .click();
    const single = page.getByRole('radio', { name: 'Manche unique' });
    await single.click();
    await expect(single).toHaveAttribute('aria-checked', 'true');
    await page.getByRole('button', { name: 'Jouer', exact: true }).click();

    // La partie démarre directement : ma main, deux adversaires qui portent le badge « Bot »
    await expect(alice.handList).toBeVisible();
    await expect(alice.cards).toHaveCount(7);
    await expect(page.locator('app-opponent-seat .bot-badge')).toHaveCount(2);

    const finished = () =>
      page.getByRole('heading', { name: /gagne la manche|remporte la partie/ }).isVisible();
    await playUntil([alice], finished);
    await expect(
      page.getByRole('heading', { name: /gagne la manche|remporte la partie/ }),
    ).toBeVisible();

    await context.close();
  });

  test('l’hôte ajoute un bot au salon : il est prêt d’office et la partie se lance', async ({
    browser,
    stack,
  }) => {
    const { context, alice } = await newAlice(browser, stack.url);
    await alice.createRoom();

    await alice.page
      .getByRole('radiogroup', { name: 'Niveau du bot' })
      .getByRole('radio', { name: 'Facile' })
      .click();
    await alice.page.getByRole('button', { name: 'Ajouter un bot' }).click();
    await expect(alice.page.locator('.bot-badge')).toHaveCount(1);

    // Aucun « Prêt » à attendre : le bot l'est toujours
    const start = alice.page.getByRole('button', { name: 'Lancer la partie' });
    await expect(start).toHaveAttribute('aria-disabled', 'false');
    await start.click();
    await expect(alice.cards).toHaveCount(7);

    await context.close();
  });
});
