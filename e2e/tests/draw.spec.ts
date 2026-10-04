import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { type Actor, playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

test.describe('pioche jusqu’à pouvoir jouer', () => {
  test.use({ seed: SEEDS.multiDraw, drawAmount: 'untilPlayable' });

  test('un joueur qui ne peut rien jouer pioche plusieurs cartes d’un coup, les autres n’en voient que le nombre', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    // Après sa pioche, Alice doit choisir (la dernière carte est spéciale) : c'est là qu'on arrête de jouer pour elle
    const mustChoose = host.deck.and(host.page.getByLabel('Garder la carte'));
    const alice: Actor = {
      canAct: async () => !(await mustChoose.isVisible()) && host.canAct(),
      actNow: async () => !(await mustChoose.isVisible()) && host.actNow(),
    };
    await playUntil([guest, alice], async () => mustChoose.isVisible());

    await expect(host.cards).toHaveCount(8);
    await expect(guest.cardCountOf('Alice')).toHaveText('8');
  });
});

test.describe('pioche rythmée par le serveur', () => {
  test.use({ seed: SEEDS.multiDraw, drawAmount: 'untilPlayable', drawStepMs: 400 });

  test('les autres voient le compteur d’Alice monter d’une carte à la fois', async ({ lobby }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    const mustChoose = host.deck.and(host.page.getByLabel('Garder la carte'));
    const alice: Actor = {
      canAct: async () => !(await mustChoose.isVisible()) && host.canAct(),
      actNow: async () => !(await mustChoose.isVisible()) && host.actNow(),
    };

    // Le compteur n'est pas lu à une date fixe : un relevé tourne pendant la partie et note chaque valeur qu'il prend
    const seen: number[] = [];
    let playing = true;
    const sampler = (async () => {
      while (playing) {
        const count = Number(await guest.cardCountOf('Alice').textContent());
        if (seen.at(-1) !== count) {
          seen.push(count);
        }
        await guest.page.waitForTimeout(30);
      }
    })();
    await playUntil([guest, alice], async () => mustChoose.isVisible());
    await expect(host.cards).toHaveCount(8);
    await expect(guest.cardCountOf('Alice')).toHaveText('8');
    playing = false;
    await sampler;

    // Les cartes arrivent une à une : le compteur ne saute jamais une valeur en montant, et monte au moins trois fois
    const rises = seen.filter((count, index) => index > 0 && count > (seen[index - 1] ?? count));
    expect(rises.length).toBeGreaterThanOrEqual(3);
    expect(
      seen.every(
        (count, index) =>
          index === 0 || count < (seen[index - 1] ?? 0) || count === (seen[index - 1] ?? 0) + 1,
      ),
    ).toBe(true);
  });
});
