import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

async function playWildDrawFour(lobby: Parameters<typeof startMatch>[0]) {
  const { host, guest } = lobby;
  await startMatch(lobby);
  await guest.card('Joker plus quatre, jouable').click();
  await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();
  await expect(host.page.getByRole('dialog', { name: 'Joker +4 contre toi !' })).toBeVisible();
  await expect(host.challengeButton).toBeVisible();
}

test.describe('+4 contesté à raison', () => {
  test.use({ seed: SEEDS.bluffedWildDrawFour });

  test('un bluff découvert fait piocher 4 cartes à celui qui a posé le +4', async ({ lobby }) => {
    const { host, guest } = lobby;
    await playWildDrawFour(lobby);

    await host.challengeButton.click();

    for (const player of [host, guest]) {
      await expect(
        player.page.getByRole('status').filter({ hasText: 'c’était un bluff' }),
      ).toContainText('Alice conteste : c’était un bluff, Bob pioche 4.');
    }
    // Bob avait 7 cartes, en a posé une et en pioche 4 ; Alice n'a rien pioché
    await expect(host.cardCountOf('Bob')).toHaveText('10');
    await expect(host.cardCountOf('Alice')).toHaveText('7');
  });
});

test.describe('+4 contesté à tort', () => {
  test.use({ seed: SEEDS.legalWildDrawFour });

  test('contester un +4 légal fait piocher 6 cartes au contestataire', async ({ lobby }) => {
    const { host, guest } = lobby;
    await playWildDrawFour(lobby);

    await host.challengeButton.click();

    for (const player of [host, guest]) {
      await expect(
        player.page.getByRole('status').filter({ hasText: 'conteste à tort' }),
      ).toContainText('Alice conteste à tort et pioche 6.');
    }
    await expect(host.cardCountOf('Alice')).toHaveText('13');
    await expect(host.cardCountOf('Bob')).toHaveText('6');
  });

  test('accepter un +4 fait piocher 4 cartes sans verdict', async ({ lobby }) => {
    const { host } = lobby;
    await playWildDrawFour(lobby);

    await host.acceptPenalty.click();

    await expect(host.cardCountOf('Alice')).toHaveText('11');
    await expect(host.page.getByRole('status').filter({ hasText: 'conteste' })).toHaveCount(0);
  });
});
