import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

test.describe('poser une carte', () => {
  test.use({ seed: SEEDS.quickGame });

  test('la carte quitte la main, arrive sur la défausse et le tour passe', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    // Un seul coup possible : toutes les autres cartes sont désactivées
    await expect(guest.playableCards).toHaveCount(1);
    const label = ((await guest.playableCards.first().getAttribute('aria-label')) ?? '').replace(
      ', jouable',
      '',
    );
    await guest.playableCards.first().click();

    for (const player of [host, guest]) {
      await expect(
        player.page.locator('[data-anchor="discard"]').getByRole('img', { name: label }),
      ).toBeVisible();
      await expect(player.cardCountOf('Bob')).toHaveText('6');
    }
    await expect(host.myTurn).toBeVisible();
    await expect(guest.myTurn).toBeHidden();
    await expect(guest.cards).toHaveCount(6);
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
  test.use({ seed: SEEDS.guidedDraw });

  test('un joueur qui ne peut rien jouer voit le serveur piocher à sa place', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    // Bob joue sa seule carte jouable (la politique des tests) : Alice n'a rien à répondre
    await playUntil([guest], async () => host.myTurn.isVisible());

    // Alice ne clique sur rien : le serveur pioche pour elle après un court instant
    await expect(host.journal).toContainText('Alice pioche 1 carte.');
    await expect(guest.journal).toContainText('Alice pioche 1 carte.');
  });
});
