import type { Locator, Page } from '@playwright/test';
import { expect, test } from '../support/fixtures.ts';
import { Player } from '../support/player.ts';

// Le jeu au clavier seul (étape 3.7) : de l'accueil à la fin d'une manche contre 1 bot, sans une seule souris. Chaque
// élément atteint par Tab doit porter un anneau de focus visible.

const MAX_TABS = 80;

/**
 * Appuie sur Tab jusqu'à ce que `target` ait le focus ; vérifie son anneau de focus. Échoue après `MAX_TABS`.
 * Avec `mayVanish`, un élément qui disparaît pendant la recherche (le jeu a avancé : le tour a changé, une carte a été jouée
 * à la place du joueur) n'est pas une erreur : on rend `false` et l'appelant regarde l'écran de nouveau. Sans lui, un
 * élément absent fait échouer le test au lieu de l'attendre sans fin.
 */
async function tabTo(
  page: Page,
  target: Locator,
  { mayVanish = false }: { mayVanish?: boolean } = {},
): Promise<boolean> {
  const element = target.first();
  for (let presses = 0; presses < MAX_TABS; presses++) {
    if (mayVanish && (await target.count()) === 0) {
      return false;
    }
    const focused = await element
      .evaluate((node) => node === document.activeElement, undefined, { timeout: 2000 })
      .catch(() => false);
    if (focused) {
      const ring = await element.evaluate((node) => {
        const style = getComputedStyle(node);
        return style.outlineStyle !== 'none' && parseFloat(style.outlineWidth) > 0
          ? ''
          : `${node.tagName}.${node.className} sans anneau de focus`;
      });
      expect(ring, 'anneau de focus visible').toBe('');
      return true;
    }
    await page.keyboard.press('Tab');
  }
  throw new Error(`Le focus n’atteint jamais l’élément en ${MAX_TABS} Tab`);
}

test.describe('clavier seul', () => {
  test.use({ seed: 5 });

  test('une manche contre 1 bot, de l’accueil à la fin, sans souris', async ({
    browser,
    stack,
  }) => {
    // Une manche entière au clavier prend de 1,5 à 2 minutes (chaque coup demande plusieurs Tab) : marge
    test.setTimeout(180_000);
    const context = await browser.newContext({
      baseURL: stack.url,
      viewport: { width: 1280, height: 720 },
      locale: 'fr-FR',
      reducedMotion: 'reduce',
    });
    const player = new Player(await context.newPage(), 'Alice');
    const { page } = player;

    await page.goto('/');
    await page.getByLabel('Ton pseudo').fill('Alice');
    await tabTo(page, page.getByRole('button', { name: /Jouer contre des bots/ }));
    await page.keyboard.press('Enter');
    await tabTo(
      page,
      page.getByRole('radiogroup', { name: 'Nombre de bots' }).getByRole('radio', { name: '1' }),
    );
    await page.keyboard.press('Space');
    await tabTo(page, page.getByRole('radio', { name: 'Manche unique' }));
    await page.keyboard.press('Space');
    await tabTo(page, page.getByRole('button', { name: 'Jouer', exact: true }));
    await page.keyboard.press('Enter');
    await expect(player.handList).toBeVisible();

    const finished = page.getByRole('heading', { name: /gagne la manche|remporte la partie/ });
    let plays = 0;
    while (!(await finished.isVisible())) {
      if (await player.colorPicker.isVisible()) {
        // La roue de couleur : les touches 1 à 4 choisissent
        await page.keyboard.press('1');
      } else if (await player.acceptPenalty.isVisible()) {
        if (await tabTo(page, player.acceptPenalty, { mayVanish: true })) {
          await page.keyboard.press('Enter');
        }
      } else if (await player.targetPicker.isVisible()) {
        if (await tabTo(page, player.targetPicker.locator('.target'), { mayVanish: true })) {
          await page.keyboard.press('Enter');
        }
      } else if (await player.canAct()) {
        const before = await player.snapshot();
        // Si le tour a changé pendant la recherche, il n'y a plus de carte à jouer : on regarde l'écran de nouveau
        if (await tabTo(page, player.playableCards, { mayVanish: true })) {
          await page.keyboard.press('Enter');
          plays++;
          await expect.poll(() => player.snapshot()).not.toBe(before);
        }
      } else {
        // Rien à jouer : le jeu pioche ou un bot joue ; on attend que l'écran offre un coup ou que la manche finisse
        await expect
          .poll(async () => (await finished.isVisible()) || (await player.canAct()))
          .toBe(true);
      }
    }
    expect(plays).toBeGreaterThan(0);
    await context.close();
  });
});
