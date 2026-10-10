import { expect, test } from '../support/fixtures.ts';

// La cinématique d'introduction : les tests pilotés par un robot ne la voient pas (navigator.webdriver), ici on la
// demande en se faisant passer pour un vrai navigateur.

test.describe('Cinématique d’introduction', () => {
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      Object.defineProperty(Navigator.prototype, 'webdriver', { get: () => false });
    });
  });

  test('elle recouvre l’accueil à la première ouverture, se passe d’un clic et ne revient pas de la session', async ({
    page,
    stack,
  }) => {
    await page.goto(stack.url);
    const intro = page.locator('app-intro .intro');

    await expect(intro).toBeVisible();
    await intro.click();
    await expect(intro).toHaveCount(0);
    await expect(page.getByRole('button', { name: /Jouer contre des bots/ })).toBeVisible();

    await page.reload();
    await expect(page.getByRole('button', { name: /Jouer contre des bots/ })).toBeVisible();
    await expect(page.locator('app-intro .intro')).toHaveCount(0);
  });

  test('elle s’efface d’elle-même en moins de trois secondes', async ({ page, stack }) => {
    await page.goto(stack.url);

    await expect(page.locator('app-intro .intro')).toBeVisible();
    await expect(page.locator('app-intro .intro')).toHaveCount(0, { timeout: 4000 });
  });

  test('une touche suffit à la passer, et le son ne démarre qu’après ce geste', async ({
    page,
    stack,
  }) => {
    await page.goto(stack.url);
    await expect(page.locator('app-intro .intro')).toBeVisible();

    await page.keyboard.press('Space');

    await expect(page.locator('app-intro .intro')).toHaveCount(0);
    await expect(page.getByRole('button', { name: 'Son activé' })).toBeVisible();
  });
});

test('le réglage du son se garde d’une visite à l’autre', async ({ page, stack }) => {
  await page.goto(stack.url);
  const toggle = page.getByRole('button', { name: /^Son (activé|coupé)/ });
  await expect(toggle).toHaveText(/Son activé/);

  await toggle.click();
  await expect(toggle).toHaveText(/Son coupé/);
  await page.reload();

  await expect(page.getByRole('button', { name: /^Son (activé|coupé)/ })).toHaveText(/Son coupé/);
});
