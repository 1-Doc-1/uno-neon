import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

// Les captures du README et du mode d'emploi (docs/img/), refaites à la demande : UNO_E2E_SCREENSHOTS=1 npx playwright
// test screenshots. Sans cette variable, ce fichier ne fait rien (jamais en CI). Les captures de la table viennent de
// `responsive.spec.ts`.

const SHOT = process.env['UNO_E2E_SCREENSHOTS'] === '1';
const VIEWPORT = { width: 1440, height: 900 };

test.describe('captures du README', () => {
  test.skip(!SHOT, 'génère les captures de docs/img seulement à la demande');

  test.describe('accueil, salon et règles', () => {
    test.use({ seed: SEEDS.quickGame });

    test('accueil, salon, comment jouer', async ({ lobby, browser, stack }) => {
      const { host } = lobby;
      // Le salon est plus haut qu'un écran : une fenêtre assez haute pour y voir tous les réglages
      await host.page.setViewportSize({ width: 1440, height: 1500 });
      await host.page.evaluate(() => window.scrollTo(0, 0));
      await host.page.screenshot({ path: '../docs/img/salon-1440.png' });

      const context = await browser.newContext({
        baseURL: stack.url,
        viewport: VIEWPORT,
        locale: 'fr-FR',
        reducedMotion: 'reduce',
      });
      const page = await context.newPage();
      await page.goto('/');
      const gate = page.getByRole('button', { name: /Cliquer pour jouer/ });
      if (await gate.isVisible().catch(() => false)) {
        await gate.click();
      }
      await page.getByLabel('Ton pseudo').fill('Alice');
      await page.screenshot({ path: '../docs/img/accueil-1440.png' });

      await page.goto('/comment-jouer');
      await expect(page.getByRole('heading', { name: 'Comment jouer' })).toBeVisible();
      await page.screenshot({ path: '../docs/img/comment-jouer-1440.png' });
      await context.close();
    });
  });

  test.describe('fin de partie', () => {
    test.use({ seed: SEEDS.fastGame });

    test('le podium', async ({ lobby }) => {
      const { host, guest } = lobby;
      await chooseSetting(host, 'Manche unique');
      await startMatch(lobby);
      await guest.page.setViewportSize(VIEWPORT);
      await playUntil([guest, host], async () =>
        guest.page.getByRole('dialog', { name: 'Podium de la partie' }).isVisible(),
      );
      // Le temps que les scores aient fini de défiler
      await guest.page.waitForTimeout(2500);
      await guest.page.screenshot({ path: '../docs/img/podium-1440.png' });
    });
  });
});
