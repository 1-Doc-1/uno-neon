import { expect, test } from '../support/fixtures.ts';
import { chooseMultiplier, chooseSetting, startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

// Le cumul des pénalités à l'échelle (ADR 0029), dans un salon dont le paquet est riche en +2 et en +4. Bob (l'invité)
// joue le premier.

test.describe('Cumul à l’échelle', () => {
  test.use({ seed: SEEDS.ladderChain });

  test('un +2, un +2 dessus, un +4 dessus : Alice, qui ne peut plus empiler, pioche les huit cartes', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Échelle');
    await chooseMultiplier(host, 'Nombre de cartes +2', '×5');
    await chooseMultiplier(host, 'Nombre de jokers +4', '×5');
    await startMatch(lobby);

    // Bob pose son +2 jaune : Alice ne le pioche pas, elle le doit
    await guest.card(/^Plus deux jaune/).click();
    await expect(host.deck).toHaveAttribute('aria-label', 'Piocher 2 cartes');
    // Aucune fenêtre ne s'ouvre : seules ses cartes qui peuvent s'empiler (ses +2) sont jouables
    await expect(host.page.getByRole('region', { name: 'Un Joker +5 te vise' })).toHaveCount(0);
    await expect(host.page.getByRole('dialog', { name: /Contester/ })).toHaveCount(0);
    await expect(host.card(/^Plus deux/)).toBeEnabled();
    await expect(host.card(/^\d/)).toBeDisabled();

    // Alice empile son +2 : la couleur ne compte pas
    await host.card(/^Plus deux/).click();
    await expect(guest.deck).toHaveAttribute('aria-label', 'Piocher 4 cartes');

    // Bob empile un +4 : un niveau au-dessus, le total passe à huit
    await guest.card(/^Joker plus quatre/).click();
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();
    await expect(host.page.locator('app-piles .penalty')).toHaveText('+8');

    // Alice n'a rien pour empiler : le serveur pioche pour elle, elle perd son tour
    await expect(host.cardCountOf('Alice')).toHaveText('14');
    await expect(guest.cardCountOf('Alice')).toHaveText('14');
    await expect(guest.cardCountOf('Bob')).toHaveText('5');
    await expect(guest.myTurn).toBeVisible();
  });
});
