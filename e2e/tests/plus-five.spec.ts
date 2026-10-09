import { expect, test } from '../support/fixtures.ts';
import { chooseMultiplier, startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

// Le Joker +5 (ADR 0028), dans un salon dont le paquet est riche en +5 (×5). Bob (l'invité) joue le premier.

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

test.describe('Joker +5 répliqué par un Joker +5', () => {
  test.use({ seed: SEEDS.plusFiveAnswered });

  test('Alice répond avec le sien en visant Bob : le total passe à dix et c’est Bob qui pioche', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseMultiplier(host, 'Nombre de jokers +5', '×5');
    await startMatch(lobby);

    // Bob vise Alice, qui a un Joker +5 : elle choisit (le serveur ne joue pas pour elle)
    await guest.card(/^Joker plus cinq/).click();
    await guest.targetPicker.getByRole('button', { name: /Alice/ }).click();
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();
    const prompt = host.page.getByRole('region', { name: 'Un Joker +5 te vise' });
    await expect(prompt).toBeVisible();
    await expect(prompt).toContainText('Tu dois piocher 5 cartes');
    await expect(prompt).toContainText('réponds avec ton Joker +5');

    // Alice répond : même geste, cible Bob (par un clic sur son siège), couleur rouge
    await host.card(/^Joker plus cinq/).click();
    await expect(host.targetPicker).toBeVisible();
    await host.page.getByRole('button', { name: 'Viser Bob' }).click();
    await host.colorPicker.getByRole('button', { name: 'Rouge' }).click();

    for (const player of [host, guest]) {
      await expect(player.journal).toContainText('Alice vise Bob : 10 cartes à piocher.');
    }
    // Bob n'a plus de Joker +5 : le serveur accepte pour lui, il pioche les dix cartes et le tour passe à Alice
    await expect(guest.cardCountOf('Bob')).toHaveText('16');
    await expect(host.cardCountOf('Bob')).toHaveText('16');
    await expect(host.cardCountOf('Alice')).toHaveText('6');
    await expect(host.myTurn).toBeVisible();
  });
});
