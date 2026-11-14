import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

// Joker +4 strict (ADR 0041) : le serveur refuse un +4 quand la main a une carte de la couleur courante. Il n'y a donc
// plus de bluff, plus de contestation : un +4 posé est toujours légal et se pioche tout de suite. Alice est l'hôte, Bob
// l'invité (qui joue en premier avec ces graines).

test.describe('+4 strict, main avec une carte de la couleur courante', () => {
  test.use({ seed: SEEDS.bluffedWildDrawFour });

  test('le +4 n’est pas jouable : l’interface ne l’offre pas, un autre coup reste possible', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Strict');
    await startMatch(lobby);

    const wildDrawFour = guest.handList.getByRole('button', {
      name: 'Joker plus quatre',
      exact: true,
    });
    await expect(wildDrawFour).toBeVisible();
    await expect(wildDrawFour).toBeDisabled();
    await expect(guest.playableCards.first()).toBeVisible();
  });

  test('sans le mode strict, le même +4 se joue en bluff', async ({ lobby }) => {
    const { guest } = lobby;
    await startMatch(lobby);

    await expect(guest.card('Joker plus quatre, jouable')).toBeEnabled();
  });
});

test.describe('+4 strict, main sans carte de la couleur courante', () => {
  test.use({ seed: SEEDS.legalWildDrawFour });

  test('un +4 légal se pioche tout de suite : aucune fenêtre de contestation', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Strict');
    await startMatch(lobby);

    await guest.card('Joker plus quatre, jouable').click();
    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();

    // Alice pioche ses 4 cartes sans rien décider : ni « Contester » ni « Accepter »
    await expect(host.cardCountOf('Alice')).toHaveText('11');
    await expect(host.cardCountOf('Bob')).toHaveText('6');
    await expect(host.page.getByRole('dialog', { name: 'Joker +4 contre toi !' })).toHaveCount(0);
    await expect(host.challengeButton).toHaveCount(0);
    await expect(host.acceptPenalty).toHaveCount(0);
  });
});

test.describe('+4 strict, réglage du salon', () => {
  test('l’invité voit le mode choisi par l’hôte', async ({ lobby }) => {
    const { host, guest } = lobby;
    await expect(guest.page.getByText('Joker +4 contestable')).toBeVisible();

    await chooseSetting(host, 'Strict');

    await expect(guest.page.getByText('Joker +4 strict')).toBeVisible();
  });
});
