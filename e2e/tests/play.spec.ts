import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

test.describe('poser une carte', () => {
  test.use({ seed: SEEDS.quickGame });

  test('la carte quitte la main, arrive sur la défausse et le tour passe', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await expect(guest.card('1 jaune, jouable')).toBeEnabled();
    await expect(guest.card('8 vert')).toBeDisabled(); // pas de la couleur active, pas le même chiffre
    await guest.card('1 jaune, jouable').click();

    for (const player of [host, guest]) {
      await expect(
        player.page.locator('[data-anchor="discard"]').getByRole('img', { name: '1 jaune' }),
      ).toBeVisible();
      await expect(player.cardCountOf('Bob')).toHaveText('6');
    }
    await expect(host.myTurn).toBeVisible();
    await expect(guest.myTurn).toBeHidden();
    await expect(guest.card('1 jaune')).toHaveCount(0);
  });
});

test.describe('Joker', () => {
  test.use({ seed: SEEDS.jokerInHand });

  test('le Joker demande une couleur, qui devient la couleur active pour les deux joueurs', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await guest.card('Joker, jouable').click();
    await expect(guest.colorPicker).toBeVisible();
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();

    for (const player of [host, guest]) {
      await expect(player.page.locator('app-piles .color')).toContainText('Bleu');
      await expect(player.cardCountOf('Bob')).toHaveText('6');
    }
    await expect(guest.colorPicker).toBeHidden();
    await expect(host.myTurn).toBeVisible();
    // Alice ne peut jouer que du bleu ou un Joker
    await expect(host.playableCards.first()).toBeEnabled();
  });
});

test.describe('pioche guidée', () => {
  test.use({ seed: SEEDS.quickGame });

  test('un joueur qui ne peut rien jouer voit le serveur piocher à sa place', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    // Bob joue son 1 jaune (la politique des tests) : Alice n'a ni jaune ni 1
    await playUntil([guest], async () => host.myTurn.isVisible());
    await expect(host.page.locator('app-piles .color')).toContainText('Jaune');

    // Alice ne clique sur rien : le serveur pioche pour elle après un court instant
    await expect(host.journal).toContainText('Alice pioche 1 carte.');
    await expect(guest.journal).toContainText('Alice pioche 1 carte.');
  });
});
