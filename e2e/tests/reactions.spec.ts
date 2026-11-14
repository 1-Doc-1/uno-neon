import { expect, test } from '../support/fixtures.ts';
import { axeViolationsAtRest } from '../support/axe-at-rest.ts';
import { startMatch } from '../support/flow.ts';
import { Player } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

// Les réactions rapides (étape 5.2) : six pictogrammes prédéfinis, une bulle sur le siège de l'émetteur, un envoi toutes
// les 2 s au plus (SPEC §8). Alice est l'hôte, Bob l'invité.

test.describe('réactions rapides', () => {
  test.use({ seed: SEEDS.quickGame });

  test('Bob réagit d’un clic : la bulle apparaît sur son siège chez Alice et sur sa pastille, puis disparaît', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await guest.page.getByRole('button', { name: 'Réagir' }).click();
    await guest.page.getByRole('button', { name: 'Bien joué' }).click();

    await expect(
      host.page.locator('app-opponent-seat', { hasText: 'Bob' }).locator('.bubble'),
    ).toHaveText('👏');
    await expect(
      host.page.getByRole('status').filter({ hasText: 'Bob : Bien joué' }),
    ).toBeAttached();
    await expect(guest.page.locator('app-my-badge .bubble')).toHaveText('👏');
    await expect(host.page.locator('app-opponent-seat .bubble')).toBeHidden();
    await expect(guest.page.locator('app-my-badge .bubble')).toBeHidden();
  });

  test('le serveur n’accepte qu’une réaction toutes les 2 s : les choix sont grisés entre-temps', async ({
    lobby,
  }) => {
    const { guest } = lobby;
    await startMatch(lobby);
    await guest.page.getByRole('button', { name: 'Réagir' }).click();
    await guest.page.getByRole('button', { name: 'Wow' }).click();

    await guest.page.getByRole('button', { name: 'Réagir' }).click();
    await expect(guest.page.getByRole('button', { name: 'En feu' })).toBeDisabled();
    await expect(guest.page.getByRole('button', { name: 'En feu' })).toBeEnabled();
  });

  test('au clavier seul et sans violation axe, sélecteur ouvert', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await guest.page.getByRole('button', { name: 'Réagir' }).focus();
    await guest.page.keyboard.press('Enter');
    expect(await axeViolationsAtRest(guest.page), 'axe, sélecteur ouvert').toEqual([]);
    await guest.page.keyboard.press('Tab');
    await guest.page.keyboard.press('Tab');
    await guest.page.keyboard.press('Enter');

    await expect(host.page.getByRole('status').filter({ hasText: 'Bob : Wow' })).toBeAttached();
    await expect(guest.page.getByRole('button', { name: 'Wow' })).toBeHidden();
  });

  test('Échap replie le sélecteur sans rien envoyer', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    await guest.page.getByRole('button', { name: 'Réagir' }).click();
    await expect(guest.page.getByRole('group', { name: 'Réactions rapides' })).toBeVisible();

    await guest.page.keyboard.press('Escape');

    await expect(guest.page.getByRole('group', { name: 'Réactions rapides' })).toBeHidden();
    await expect(host.page.locator('app-opponent-seat .bubble')).toHaveCount(0);
  });
});

test.describe('réactions et bots', () => {
  test.use({ seed: 5 });

  test('contre des bots, ma réaction s’affiche et aucun bot n’en envoie', async ({
    browser,
    stack,
  }) => {
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
    await page.getByRole('button', { name: /Jouer contre des bots/ }).click();
    await page
      .getByRole('radiogroup', { name: 'Nombre de bots' })
      .getByRole('radio', { name: '2' })
      .click();
    await page.getByRole('button', { name: 'Jouer', exact: true }).click();
    await expect(player.handList).toBeVisible();

    await page.getByRole('button', { name: 'Réagir' }).click();
    await page.getByRole('button', { name: 'En feu' }).click();
    await expect(page.locator('app-my-badge .bubble')).toHaveText('🔥');

    // Le temps que ma bulle s'efface, les bots ont joué : aucune bulle n'est apparue sur leurs sièges
    await expect(page.locator('app-my-badge .bubble')).toBeHidden();
    await expect(page.locator('app-opponent-seat .bubble')).toHaveCount(0);
    await context.close();
  });
});
