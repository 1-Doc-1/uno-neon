import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import type { Player } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

// L'option 7-0 (ADR 0041) à deux joueurs : un 7 échange la main de Bob avec celle d'Alice, un 0 aussi (deux mains qui
// tournent d'un cran, c'est un échange). Alice est l'hôte, Bob l'invité, qui joue en premier. Le choix d'un joueur parmi
// plusieurs est couvert par les tests de la table (3 joueurs) et du serveur.

/** La main sous forme de cartes, sans l'état « jouable » (qui dépend du tour). */
async function hand(player: Player): Promise<string[]> {
  const labels = await player.handLabels();
  return labels.map((label) => label.replace(', jouable', '')).sort();
}

test.describe('7 : échange des mains', () => {
  test.use({ seed: SEEDS.sevenFirst });

  test('Bob pose un 7 : chacun a la main de l’autre, et le journal le dit', async ({ lobby }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, '7 et 0');
    await startMatch(lobby);
    const bobBefore = await hand(guest);
    const aliceBefore = await hand(host);

    await guest.card('7 jaune, jouable').click();

    // Alice avait 7 cartes, Bob en a posé une : 6 ; après l'échange, c'est l'inverse
    await expect(host.cardCountOf('Alice')).toHaveText('6');
    await expect(host.cardCountOf('Bob')).toHaveText('7');
    expect(await hand(host)).toEqual(bobBefore.filter((label) => label !== '7 jaune'));
    expect(await hand(guest)).toEqual(aliceBefore);
    for (const player of [host, guest]) {
      await expect(player.journal).toContainText('Bob échange sa main avec Alice.');
    }
  });

  test('sans l’option, le 7 est une carte ordinaire : rien ne s’échange', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    const aliceBefore = await hand(host);

    await guest.card('7 jaune, jouable').click();

    await expect(host.cardCountOf('Bob')).toHaveText('6');
    expect(await hand(host)).toEqual(aliceBefore);
    await expect(host.journal).not.toContainText('échange sa main');
  });

  test('la main change de siège sous les yeux d’Alice : des cartes de dos volent entre les deux', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, '7 et 0');
    await startMatch(lobby);

    await guest.card('7 jaune, jouable').click();

    // Les cartes de l'échange sont de dos seulement (la carte posée par Bob a aussi sa face) : on ne regarde qu'elles
    const swapCards = host.page.locator(
      'app-effects-layer .flight:has(app-card-back):not(:has(app-card))',
    );
    await expect(swapCards.first()).toBeVisible();
    await expect(swapCards).toHaveCount(0);
    await expect(host.cardCountOf('Alice')).toHaveText('6');
  });
});

test.describe('0 : les mains tournent', () => {
  test.use({ seed: SEEDS.zeroFirst });

  test('à deux joueurs, un 0 échange les deux mains', async ({ lobby }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, '7 et 0');
    await startMatch(lobby);
    const bobBefore = await hand(guest);
    const aliceBefore = await hand(host);

    await guest.card('0 rouge, jouable').click();

    await expect(host.cardCountOf('Alice')).toHaveText('6');
    expect(await hand(host)).toEqual(bobBefore.filter((label) => label !== '0 rouge'));
    expect(await hand(guest)).toEqual(aliceBefore);
    await expect(host.journal).toContainText('Les mains tournent.');
  });
});

test.describe('réglage du salon', () => {
  test('l’invité voit que l’échange de mains est activé', async ({ lobby }) => {
    const { host, guest } = lobby;
    await expect(guest.page.getByText('7 et 0 : échange de mains')).toHaveCount(0);

    await chooseSetting(host, '7 et 0');

    await expect(guest.page.getByText('7 et 0 : échange de mains')).toBeVisible();
  });
});
