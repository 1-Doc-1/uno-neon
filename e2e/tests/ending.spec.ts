import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

test.use({ seed: SEEDS.fastGame });

test('manche unique : la partie se termine quand Bob pose sa dernière carte', async ({ lobby }) => {
  const { host, guest } = lobby;
  await chooseSetting(host, 'Manche unique');
  await startMatch(lobby);

  const finished = () =>
    guest.page.getByRole('heading', { name: /gagne la manche|remporte la partie/ }).isVisible();
  await playUntil([guest, host], finished);

  for (const player of [host, guest]) {
    await expect(
      player.page.getByRole('heading', { name: 'Bob remporte la partie !' }),
    ).toBeVisible();
  }
  await expect(guest.cardCountOf('Bob')).toHaveText('0');
});
