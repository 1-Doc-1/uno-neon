import { expect, test } from "../support/fixtures.ts";
import { startMatch } from "../support/flow.ts";
import { type Actor, playUntil } from "../support/player.ts";
import { SEEDS } from "../support/seeds.ts";

test.describe("pioche jusqu’à pouvoir jouer", () => {
  test.use({ seed: SEEDS.multiDraw, drawAmount: "untilPlayable" });

  test("un joueur qui ne peut rien jouer pioche plusieurs cartes d’un coup, les autres n’en voient que le nombre", async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    // Après sa pioche, Alice doit choisir (la dernière carte est spéciale) : c'est là qu'on arrête de jouer pour elle
    const mustChoose = host.deck.and(host.page.getByLabel("Garder la carte"));
    const alice: Actor = {
      canAct: async () => !(await mustChoose.isVisible()) && host.canAct(),
      actNow: async () => !(await mustChoose.isVisible()) && host.actNow(),
    };
    await playUntil([guest, alice], async () => mustChoose.isVisible());

    // Une entrée du journal par carte piochée (il garde les quatre dernières), les mêmes pour tout le monde
    for (const player of [host, guest]) {
      await expect(
        player.journal.getByText("Alice pioche 1 carte.").nth(1),
      ).toBeVisible();
    }
    await expect(host.cards).toHaveCount(8);
    await expect(guest.cardCountOf("Alice")).toHaveText("8");
  });
});
