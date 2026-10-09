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
  test.use({ seed: SEEDS.multiDraw, drawAmount: 'untilPlayable', paceMs: 400 });

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

test.describe('main pendant une pioche, à vitesse réelle', () => {
  // Animations activées et rythme de production (1 s par carte) : c'est là que les cartes volantes se voient vraiment
  test.use({ seed: SEEDS.multiDraw, drawAmount: 'untilPlayable', paceMs: 1000, motion: 'no-preference' });
  test.setTimeout(60_000);

  test('les cartes piochées rejoignent la main, sans couche volante ni dos qui reste', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    const mustChoose = host.deck.and(host.page.getByLabel('Garder la carte'));
    const alice: Actor = {
      canAct: async () => !(await mustChoose.isVisible()) && host.canAct(),
      actNow: async () => !(await mustChoose.isVisible()) && host.actNow(),
    };

    // Un relevé dans la page, à chaque image : jamais de carte volante arrivée et encore visible au-dessus de la main
    await host.page.evaluate(() => {
      const w = window as unknown as { __overlaps: string[] };
      w.__overlaps = [];
      const watch = (): void => {
        const hand = document.querySelectorAll('ul[aria-label="Ta main"] li').length;
        const landed = [...document.querySelectorAll('app-effects-layer .flight')].filter((flight) => {
          const style = getComputedStyle(flight);
          const resting = flight.getAnimations().every((animation) => animation.playState === 'finished');
          return resting && style.visibility !== 'hidden' && Number(style.opacity) > 0.9;
        });
        const target = document.querySelector('ul[aria-label="Ta main"]')?.getBoundingClientRect();
        for (const flight of landed) {
          const box = flight.getBoundingClientRect();
          if (target && box.top > target.top - 20 && box.bottom < target.bottom + 20) {
            w.__overlaps.push(`main ${hand} cartes, vol à ${Math.round(box.left)},${Math.round(box.top)}`);
          }
        }
        requestAnimationFrame(watch);
      };
      requestAnimationFrame(watch);
    });
    await playUntil([guest, alice], async () => mustChoose.isVisible());
    await expect(host.cards).toHaveCount(8);

    // Aucun élément animé ni dos de carte ne reste une fois la pioche terminée
    await expect(host.page.locator('app-effects-layer .flight')).toHaveCount(0);
    await expect(host.page.locator('app-effects-layer app-card-back')).toHaveCount(0);

    // Toutes les cartes de la main sont sur le même arc (on mesure les emplacements : une carte jouable est surélevée à dessein)
    const tops = await host.handList.locator('li').evaluateAll((slots) =>
      slots.map((slot) => slot.getBoundingClientRect().top),
    );
    expect(Math.max(...tops) - Math.min(...tops)).toBeLessThanOrEqual(8);

    const overlaps = await host.page.evaluate(
      () => (window as unknown as { __overlaps: string[] }).__overlaps,
    );
    expect(overlaps).toEqual([]);
  });
});
