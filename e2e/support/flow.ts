import { expect } from '@playwright/test';
import type { Lobby } from './fixtures.ts';
import type { Player } from './player.ts';

/** L'hôte choisit une option du salon (« Manche unique », « UNO obligatoire »…) et l'attend cochée. */
export async function chooseSetting(host: Player, option: string): Promise<void> {
  const radio = host.page.getByRole('radio', { name: option });
  await radio.click();
  await expect(radio).toHaveAttribute('aria-checked', 'true');
}

/** Bob se déclare prêt, Alice lance : les deux arrivent à la table avec leurs sept cartes. */
export async function startMatch({ host, guest }: Lobby): Promise<void> {
  await guest.page.getByRole('button', { name: 'Prêt' }).click();
  const start = host.page.getByRole('button', { name: 'Lancer la partie' });
  await expect(start).toHaveAttribute('aria-disabled', 'false');
  await start.click();
  for (const player of [host, guest]) {
    await expect(player.handList).toBeVisible();
    await expect(player.cards).toHaveCount(7);
  }
}

/** L'hôte règle le nombre d'une carte du paquet (« Nombre de jokers +5 », « ×5 ») et l'attend coché. */
export async function chooseMultiplier(host: Player, group: string, option: string): Promise<void> {
  const radio = host.page
    .getByRole('radiogroup', { name: group })
    .getByRole('radio', { name: option });
  await radio.click();
  await expect(radio).toHaveAttribute('aria-checked', 'true');
}
