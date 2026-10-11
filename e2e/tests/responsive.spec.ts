import { AxeBuilder } from '@axe-core/playwright';
import { expect, test } from '../support/fixtures.ts';
import { layoutProblems } from '../support/layout.ts';
import { Player } from '../support/player.ts';

// Mise en page de 360 à 1920 px, en portrait et en paysage (étape 3.7) : sur l'accueil, l'écran des bots et la table,
// la page ne défile jamais et aucun siège ne sort de l'écran. Avec UNO_E2E_SCREENSHOTS=1, les captures de la table
// sont écrites dans docs/img/ (jamais en CI).

const SIZES = [
  { name: '360', width: 360, height: 640 },
  { name: '360-paysage', width: 640, height: 360 },
  { name: '768', width: 768, height: 1024 },
  { name: '915-paysage', width: 915, height: 412 },
  { name: '1440', width: 1440, height: 900 },
  { name: '1920', width: 1920, height: 1080 },
];
const SHOT = process.env['UNO_E2E_SCREENSHOTS'] === '1';

test.describe('toutes les tailles d’écran', () => {
  test.use({ seed: 5 });

  for (const size of SIZES) {
    test(`la table à ${size.width}×${size.height} : rien ne défile ni ne sort de l’écran`, async ({
      browser,
      stack,
    }) => {
      const context = await browser.newContext({
        baseURL: stack.url,
        viewport: { width: size.width, height: size.height },
        locale: 'fr-FR',
        reducedMotion: 'reduce',
      });
      const player = new Player(await context.newPage(), 'Alice');
      const { page } = player;

      await page.goto('/');
      await page.getByLabel('Ton pseudo').fill('Alice');
      expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(
        size.width,
      );
      await page.getByRole('button', { name: /Jouer contre des bots/ }).click();
      expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(
        size.width,
      );
      await page
        .getByRole('radiogroup', { name: 'Nombre de bots' })
        .getByRole('radio', { name: '4' })
        .click();
      await page.getByRole('button', { name: 'Jouer', exact: true }).click();
      await expect(player.handList).toBeVisible();
      await expect(page.locator('app-opponent-seat')).toHaveCount(4);

      expect(await layoutProblems(player), 'mise en page de la table').toEqual([]);
      const axe = await new AxeBuilder({ page })
        .withTags(['wcag2a', 'wcag2aa', 'wcag22aa'])
        // Les bots jouent pendant la mesure : les effets et le journal en fondu changent d'une image à l'autre
        .exclude('app-effects-layer')
        .exclude('.journal')
        .analyze();
      expect(
        axe.violations.flatMap((violation) =>
          violation.nodes.map(
            (node) => `${violation.id}: ${node.target.join(' ')} ${node.any[0]?.message ?? ''}`,
          ),
        ),
      ).toEqual([]);
      if (SHOT) {
        await page.screenshot({ path: `../docs/img/table-${size.name}.png` });
      }
      await context.close();
    });
  }
});
