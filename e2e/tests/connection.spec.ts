import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

test.use({ seed: SEEDS.quickGame });

test('recharger l’onglet : le joueur retrouve sa main et son tour, et peut jouer', async ({
  lobby,
}) => {
  const { host, guest } = lobby;
  await startMatch(lobby);
  const handBefore = await guest.handLabels();

  await guest.page.reload();

  await expect(guest.cards).toHaveCount(7);
  expect(await guest.handLabels()).toEqual(handBefore);
  await expect(guest.myTurn).toBeVisible();
  await guest.card('5 vert, jouable').click();
  await expect(host.myTurn).toBeVisible();
  await expect(host.cardCountOf('Bob')).toHaveText('6');
});

test.describe('fermeture d’un onglet', () => {
  test.use({ reconnectGraceMs: 1500 });

  test('un joueur qui ne revient pas perd par forfait après le délai de grâce', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await host.page.close();

    await expect(guest.page.getByText('hors ligne')).toBeVisible();
    await expect(
      guest.page.getByRole('heading', { name: 'Bob remporte la partie !' }),
    ).toBeVisible();
  });
});
