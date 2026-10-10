import { AxeBuilder } from '@axe-core/playwright';
import { expect, test } from '../support/fixtures.ts';

// La page « Comment jouer » : accessible depuis l'accueil, lisible sur téléphone, sans violation d'accessibilité.

test('« Comment jouer » s’ouvre depuis l’accueil et y ramène', async ({ page, stack }) => {
  await page.goto(stack.url);

  await page.getByRole('link', { name: 'Comment jouer ?' }).click();

  await expect(page.getByRole('heading', { name: 'Comment jouer', level: 2 })).toBeVisible();
  await expect(page.getByRole('heading', { name: 'Les cartes spéciales' })).toBeVisible();
  await page.getByRole('link', { name: '← Retour à l’accueil' }).first().click();
  await expect(page.getByRole('button', { name: /Jouer contre des bots/ })).toBeVisible();
});

for (const width of [375, 1440]) {
  test(`« Comment jouer » à ${width} px : pas de défilement horizontal, axe sans violation`, async ({
    page,
    stack,
  }) => {
    await page.setViewportSize({ width, height: 900 });
    await page.goto(`${stack.url}/comment-jouer`);
    await expect(page.getByRole('heading', { name: 'Comment jouer', level: 2 })).toBeVisible();

    const overflow = await page.evaluate(
      () => document.documentElement.scrollWidth - document.documentElement.clientWidth,
    );
    expect(overflow).toBeLessThanOrEqual(0);
    const result = await new AxeBuilder({ page })
      .withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'])
      .analyze();
    expect(result.violations).toEqual([]);
  });
}
