import { expect, test } from '../support/fixtures.ts';
import { chooseMultiplier, startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

// Le Joker +5 (ADR 0028, 0034), dans un salon dont le paquet est riche en +5 (×5). Bob (l'invité) joue le premier.

test.describe('Joker +5 simple', () => {
  test.use({ seed: SEEDS.plusFiveSimple });

  test('Bob vise Alice : sans Joker +5 elle pioche cinq cartes, puis le tour revient à Bob', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseMultiplier(host, 'Nombre de jokers +5', '×5');
    await startMatch(lobby);

    await guest.card(/^Joker plus cinq/).click();
    // D'abord la cible (fenêtre non modale, les sièges restent cliquables), puis la couleur
    await expect(guest.targetPicker).toBeVisible();
    await guest.targetPicker.getByRole('button', { name: /Alice/ }).click();
    await expect(guest.colorPicker).toBeVisible();
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();

    for (const player of [host, guest]) {
      await expect(player.page.locator('app-piles .color')).toContainText('Bleu');
      await expect(player.journal).toContainText('Bob vise Alice : 5 cartes à piocher.');
    }
    // Alice n'a pas de Joker +5 : le serveur accepte pour elle (pioche guidée), ses cinq cartes arrivent, elle passe
    await expect(host.cardCountOf('Alice')).toHaveText('12');
    await expect(guest.cardCountOf('Alice')).toHaveText('12');
    await expect(guest.cardCountOf('Bob')).toHaveText('6');
    await expect(guest.myTurn).toBeVisible();
  });
});

test.describe('Joker +5 : la super carte', () => {
  test.use({ seed: SEEDS.plusFiveAnswered });

  test('Alice a un Joker +5 mais ne peut pas répondre : aucune fenêtre, elle pioche cinq cartes toute seule', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseMultiplier(host, 'Nombre de jokers +5', '×5');
    await startMatch(lobby);

    // Bob vise Alice, qui a un Joker +5 en main : cela ne change rien (ADR 0034)
    await guest.card(/^Joker plus cinq/).click();
    await guest.targetPicker.getByRole('button', { name: /Alice/ }).click();
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();
    await expect(host.page.getByRole('region', { name: 'Un Joker +5 te vise' })).toHaveCount(0);

    for (const player of [host, guest]) {
      await expect(player.journal).toContainText('Bob vise Alice : 5 cartes à piocher.');
    }
    await expect(host.cardCountOf('Alice')).toHaveText('12');
    await expect(guest.cardCountOf('Alice')).toHaveText('12');
    await expect(guest.myTurn).toBeVisible();
  });
});
