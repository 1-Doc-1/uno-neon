import AxeBuilder from '@axe-core/playwright';
import { expect, test } from '../support/fixtures.ts';

// L'écran « Cliquer pour jouer » puis la cinématique d'introduction : les tests pilotés par un robot ne la voient pas (navigator.webdriver), ici on la
// demande en se faisant passer pour un vrai navigateur.

const gate = (page: import('@playwright/test').Page) =>
  page.getByRole('button', { name: 'Cliquer pour jouer' });

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

    await expect(gate(page)).toBeVisible();
    await expect(intro).toHaveCount(0);
    await gate(page).click();
    await expect(intro).toBeVisible();
    await intro.click();
    await expect(intro).toHaveCount(0);
    await expect(page.getByRole('button', { name: /Jouer contre des bots/ })).toBeVisible();

    await page.reload();
    await expect(page.getByRole('button', { name: /Jouer contre des bots/ })).toBeVisible();
    await expect(gate(page)).toHaveCount(0);
    await expect(page.locator('app-intro .intro')).toHaveCount(0);
  });

  test('elle s’efface d’elle-même en moins de trois secondes', async ({ page, stack }) => {
    await page.goto(stack.url);
    await gate(page).click();

    await expect(page.locator('app-intro .intro')).toBeVisible();
    await expect(page.locator('app-intro .intro')).toHaveCount(0, { timeout: 4000 });
  });

  test('une touche suffit à la passer, et le son ne démarre qu’après ce geste', async ({
    page,
    stack,
  }) => {
    await page.goto(stack.url);
    await expect(gate(page)).toBeFocused();
    await page.keyboard.press('Enter');
    await expect(page.locator('app-intro .intro')).toBeVisible();

    await page.keyboard.press('Space');

    await expect(page.locator('app-intro .intro')).toHaveCount(0);
    await expect(page.getByRole('button', { name: 'Son activé' })).toBeVisible();
  });
});

test('l’écran « Cliquer pour jouer » ne présente aucune violation d’accessibilité (axe, WCAG 2.2 AA)', async ({
  page,
  stack,
}) => {
  await page.addInitScript(() => {
    Object.defineProperty(Navigator.prototype, 'webdriver', { get: () => false });
  });
  await page.goto(stack.url);
  await expect(gate(page)).toBeVisible();

  const result = await new AxeBuilder({ page })
    .withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'])
    .analyze();

  expect(result.violations).toEqual([]);
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

test('les réglages audio : deux volumes séparés et la tension de fin de tour, gardés d’une visite à l’autre', async ({
  page,
  stack,
}) => {
  await page.goto(stack.url);
  await page.getByText('Réglages audio').click();
  const effects = page.getByRole('slider', { name: 'Volume des effets' });
  const music = page.getByRole('slider', { name: 'Volume de la musique' });
  const tension = page.getByRole('checkbox', { name: 'Tension de fin de tour' });

  await expect(effects).toHaveValue('60');
  await expect(music).toHaveValue('30');
  await expect(tension).toBeChecked();

  await music.fill('70');
  await tension.uncheck();
  await page.reload();
  await page.getByText('Réglages audio').click();

  await expect(page.getByRole('slider', { name: 'Volume de la musique' })).toHaveValue('70');
  await expect(page.getByRole('slider', { name: 'Volume des effets' })).toHaveValue('60');
  await expect(page.getByRole('checkbox', { name: 'Tension de fin de tour' })).not.toBeChecked();
});
