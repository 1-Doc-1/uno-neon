import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

test.use({ seed: SEEDS.quickGame });

test('créer un salon, rejoindre par le lien, se déclarer prêt, lancer la partie', async ({
  lobby,
}) => {
  const { host, guest } = lobby;
  const start = host.page.getByRole('button', { name: 'Lancer la partie' });

  // Tant que Bob n'est pas prêt, « Lancer » est atténué et dit pourquoi
  await expect(start).toHaveAttribute('aria-disabled', 'true');
  await start.click({ force: true }); // atténué (aria-disabled), mais cliquable : le clic donne la raison
  await expect(host.page.getByRole('tooltip')).toHaveText('En attente de : Bob');

  await startMatch(lobby);

  // Les deux sont à la table : sept cartes chacun, et un seul des deux a la main
  await expect(host.cardCountOf('Alice')).toHaveText('7');
  await expect(host.cardCountOf('Bob')).toHaveText('7');
  await expect(guest.myTurn).toBeVisible();
  await expect(host.myTurn).toBeHidden();
});
