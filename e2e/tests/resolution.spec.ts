import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

test.describe('résolution d’un effet (ADR 0027)', () => {
  test.use({ seed: SEEDS.drawTwoFirst, paceMs: 500 });

  test('après un +2 à deux joueurs, le poseur ne peut pas rejouer avant la fin de la distribution', async ({
    lobby,
  }) => {
    const { guest: bob } = lobby;
    await startMatch(lobby);
    await bob.card(/^Plus deux bleu/).click();

    // Alice saute son tour : c'est de nouveau à Bob, mais sa main reste neutre pendant que les cartes arrivent chez elle
    await expect(bob.myTurn).toBeVisible();
    await expect(bob.playableCards).toHaveCount(0);
    await expect(bob.deck).toBeDisabled();
    const alice = bob.cardCountOf('Alice');
    await expect(alice).toHaveText('7');

    // Jamais de carte jouable tant qu'Alice n'a pas reçu ses deux cartes (le compteur ne fait que monter ici)
    const early: number[] = [];
    await expect
      .poll(
        async () => {
          const playable = await bob.playableCards.count();
          const count = Number(await alice.textContent());
          if (playable > 0 && count < 9) {
            early.push(count);
          }
          return playable;
        },
        { message: 'la main de Bob ne s’ouvre jamais' },
      )
      .toBeGreaterThan(0);

    expect(early).toEqual([]);
    await expect(alice).toHaveText('9');
  });
});
