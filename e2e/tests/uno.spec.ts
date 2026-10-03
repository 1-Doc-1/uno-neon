import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { type Player, playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

test.use({ seed: SEEDS.quickGame });

/** Joue (politique des tests) jusqu'à ce que Bob n'ait plus qu'une carte, sans qu'il annonce UNO. */
async function playUntilBobHasOneCard(host: Player, guest: Player): Promise<void> {
  await playUntil([guest, host], async () => (await host.cardCountOf('Bob').textContent()) === '1');
}

test('un joueur qui annonce UNO à temps ne peut pas être contré', async ({ lobby }) => {
  const { host, guest } = lobby;
  await startMatch(lobby);
  await playUntilBobHasOneCard(host, guest);

  await guest.unoButton.click();

  await expect(host.journal).toContainText('Bob annonce UNO !');
  await expect(host.page.getByText('UNO oublié !')).toBeHidden();
  await expect(host.page.getByRole('button', { name: /^Contre-UNO/ })).toHaveCount(0);
  await expect(host.cardCountOf('Bob')).toHaveText('1');
});

test('contre-UNO : une fois la grâce passée, Alice fait piocher 2 cartes à Bob qui avait oublié', async ({
  lobby,
}) => {
  const { host, guest } = lobby;
  await startMatch(lobby);
  await playUntilBobHasOneCard(host, guest);

  await expect(host.page.getByText('UNO oublié !')).toBeVisible();
  const catchButton = host.page.getByRole('button', { name: /^Contre-UNO ! Bob/ });
  // Pendant la grâce (2 s), seul Bob peut encore annoncer : le bouton d'Alice est grisé, puis il s'active
  await expect(catchButton).toHaveAttribute('aria-disabled', 'false');
  await catchButton.click();

  for (const player of [host, guest]) {
    await expect(player.cardCountOf('Bob')).toHaveText('3');
    await expect(player.journal).toContainText('Alice contre Bob : 2 cartes.');
  }
  await expect(host.page.getByText('UNO oublié !')).toBeHidden();
});

test.describe('UNO obligatoire pour gagner', () => {
  test('la dernière carte est refusée tant que Bob n’a pas annoncé UNO', async ({ lobby }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'UNO obligatoire');
    await startMatch(lobby);
    await playUntil([guest, host], async () => {
      return (
        (await guest.cardCountOf('Bob').textContent()) === '1' && (await guest.myTurn.isVisible())
      );
    });

    // La carte est refusée, avec une explication, et rien ne change
    await guest.playableCards.first().click();
    await expect(
      guest.page
        .getByRole('status')
        .filter({ hasText: 'Annonce UNO avant de poser ta dernière carte.' }),
    ).toBeVisible();
    await expect(guest.cardCountOf('Bob')).toHaveText('1');

    // Une fois UNO annoncé, la même carte passe et Bob gagne la manche
    await guest.unoButton.click();
    await guest.playableCards.first().click();
    for (const player of [host, guest]) {
      await expect(
        player.page.getByRole('heading', { name: 'Bob gagne la manche !' }),
      ).toBeVisible();
    }
  });
});
