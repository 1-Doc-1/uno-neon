// Axe, avec toutes ses règles (pas seulement WCAG AA : aussi titre de niveau 1, repères, noms de régions), sur toutes les
// pages et les principaux états de jeu : accueil, bots, règles, salon, table, réactions, chat, menu, roue des couleurs,
// contestation, podium et fin de partie. Les tests par page vérifient en plus leur propre contenu.
import { AxeBuilder } from '@axe-core/playwright';
import type { Page } from '@playwright/test';
import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

const found: string[] = [];

/** Analyse la page avec TOUTES les règles d'axe (WCAG, mais aussi les bonnes pratiques : titre de niveau 1, repères…). */
async function sweep(page: Page, where: string): Promise<void> {
  await page.locator('h1').first().waitFor({ state: 'attached' });
  const result = await new AxeBuilder({ page }).analyze();
  for (const violation of result.violations) {
    const nodes = violation.nodes.slice(0, 2).map((node) => node.target.join(' ').slice(0, 80));
    found.push(`${where} : ${violation.id} (${violation.impact}) ${nodes.join(' | ')}`);
  }
}

test.afterEach(() => {
  expect(found, 'violations axe').toEqual([]);
});

test.describe('axe, toutes les règles', () => {
  test.use({ seed: SEEDS.quickGame });

  test('pages et états', async ({ lobby }) => {
    test.setTimeout(120_000);
    const { host, guest } = lobby;
    await sweep(host.page, 'salon hôte');
    await sweep(guest.page, 'salon invité');
    await host.page.getByRole('button', { name: 'Chat' }).click();
    await sweep(host.page, 'salon chat ouvert');
    await host.page.getByRole('button', { name: 'Chat' }).click();

    await startMatch(lobby);
    await sweep(host.page, 'table hôte');
    await sweep(guest.page, 'table invité (son tour)');
    await guest.page.getByRole('button', { name: 'Réagir' }).click();
    await sweep(guest.page, 'table réactions ouvertes');
    await guest.page.keyboard.press('Escape');
    await guest.page.getByRole('button', { name: /^Chat/ }).click();
    await sweep(guest.page, 'table chat ouvert');
    await guest.page.getByRole('button', { name: /^Chat/ }).click();
    await guest.page.getByRole('button', { name: 'Menu' }).click();
    await sweep(guest.page, 'table menu de jeu');
    await guest.page.keyboard.press('Escape');

    const other = host.page;
    await other.goto('/comment-jouer');
    await sweep(other, 'comment jouer');
    await other.goto('/r/ZZZZZZ');
    await sweep(other, 'salon inconnu');
    await other.goto('/');
    await sweep(other, 'accueil');
    await other.goto('/bots');
    await sweep(other, 'bots');
  });
});

test.describe('axe, toutes les règles fin de partie', () => {
  test.use({ seed: SEEDS.fastGame });

  test('podium, fin de partie', async ({ lobby }) => {
    test.setTimeout(120_000);
    const { host, guest } = lobby;
    await chooseSetting(host, 'Manche unique');
    await startMatch(lobby);
    await playUntil([guest, host], async () =>
      guest.page.getByRole('dialog', { name: 'Podium de la partie' }).isVisible(),
    );
    await sweep(guest.page, 'podium');
    await guest.page.getByRole('button', { name: 'Passer' }).click();
    await expect(guest.page.getByRole('dialog', { name: 'Fin de la partie' })).toBeVisible();
    await sweep(guest.page, 'fin de partie');
  });
});

test.describe('axe, toutes les règles contestation', () => {
  test.use({ seed: SEEDS.bluffedWildDrawFour });

  test('roue des couleurs et contestation', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    await guest.card('Joker plus quatre, jouable').click();
    await sweep(guest.page, 'roue des couleurs');
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();
    await expect(host.page.getByRole('dialog', { name: 'Joker +4 contre toi !' })).toBeVisible();
    await sweep(host.page, 'contestation du +4');
  });
});
