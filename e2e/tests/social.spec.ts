import { AxeBuilder } from '@axe-core/playwright';
import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { Player, playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

// La session (cookie HttpOnly, un onglet actif), l'exclusion, le retour au salon, la revanche et le chat
// (ADR 0036, 0037). Alice est l'hôte, Bob l'invité.

const BUSY = 'Partie ouverte dans un autre onglet';

test.describe('session', () => {
  test.use({ seed: SEEDS.quickGame });

  test('le jeton est un cookie HttpOnly, Secure, SameSite=Strict : aucun script ne le voit', async ({
    lobby,
  }) => {
    const { host } = lobby;

    const cookies = await host.page.context().cookies();
    const session = cookies.find((cookie) => cookie.name === 'uno_session');
    expect(session).toBeDefined();
    expect(session?.httpOnly).toBe(true);
    expect(session?.secure).toBe(true);
    expect(session?.sameSite).toBe('Strict');
    const visible = await host.page.evaluate(() => ({
      cookie: document.cookie,
      local: JSON.stringify({ ...localStorage }),
      session: JSON.stringify({ ...sessionStorage }),
    }));
    expect(visible.cookie).not.toContain('uno_session');
    expect(visible.local + visible.session).not.toContain(session?.value ?? 'x');
  });

  test('fermer l’onglet puis revenir : le joueur retrouve sa place et sa main', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    const handBefore = await guest.handLabels();
    const context = guest.page.context();
    const url = guest.page.url();

    await guest.page.close();
    await expect(host.page.getByText('hors ligne')).toBeVisible();
    const page = await context.newPage();
    await page.goto(url);
    const back = new Player(page, 'Bob');

    await expect(back.cards).toHaveCount(7);
    expect(await back.handLabels()).toEqual(handBefore);
    await expect(host.page.getByText('hors ligne')).toBeHidden();
    await expect(back.myTurn).toBeVisible();
  });

  test('un seul onglet actif : le second propose de reprendre ici, et l’autre peut la reprendre', async ({
    lobby,
  }) => {
    const { guest } = lobby;
    await startMatch(lobby);
    const first = guest;
    const secondPage = await first.page.context().newPage();
    await secondPage.goto(first.page.url());
    const second = new Player(secondPage, 'Bob');

    // Le second onglet attend, le premier n'a rien perdu
    await expect(secondPage.getByRole('dialog', { name: BUSY })).toBeVisible();
    await expect(first.cards).toHaveCount(7);
    await expect(first.page.getByRole('dialog', { name: BUSY })).toBeHidden();

    // « Jouer ici » reprend la main et déconnecte l'autre onglet
    await secondPage.getByRole('button', { name: 'Jouer ici' }).click();
    await expect(second.cards).toHaveCount(7);
    await expect(first.page.getByRole('dialog', { name: BUSY })).toBeVisible();

    // Et le premier peut la reprendre à son tour
    await first.page.getByRole('button', { name: 'Jouer ici' }).click();
    await expect(first.cards).toHaveCount(7);
    await expect(secondPage.getByRole('dialog', { name: BUSY })).toBeVisible();
  });
});

test.describe('exclusion', () => {
  test.use({ seed: SEEDS.quickGame });

  test('au salon : clic sur le profil, « Exclure », puis la confirmation qui nomme le joueur', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;

    await host.page.getByRole('button', { name: 'Options de Bob' }).click();
    const menu = host.page.getByRole('dialog', { name: 'Options de Bob' });
    await menu.getByRole('button', { name: 'Exclure' }).click();
    await expect(menu.getByRole('heading', { name: 'Exclure Bob ?' })).toBeVisible();
    // Rien n'est parti tant que la confirmation n'est pas donnée
    await expect(guest.page.getByRole('heading', { name: 'Salon fermé' })).toBeHidden();
    await menu.getByRole('button', { name: 'Exclure' }).click();

    await expect(guest.page.getByRole('heading', { name: 'Salon fermé' })).toBeVisible();
    await expect(guest.page.getByText('L’hôte t’a exclu du salon.')).toBeVisible();
    await expect(host.page.getByRole('heading', { name: 'Joueurs (1/6)' })).toBeVisible();
  });

  test('l’invité n’a pas de bouton « Exclure »', async ({ lobby }) => {
    const { guest } = lobby;

    await guest.page.getByRole('button', { name: 'Options de Alice' }).click();

    const menu = guest.page.getByRole('dialog', { name: 'Options de Alice' });
    await expect(menu.getByRole('button', { name: 'Masquer ce joueur' })).toBeVisible();
    await expect(menu.getByRole('button', { name: 'Exclure' })).toHaveCount(0);
  });

  test('en partie : l’hôte exclut depuis le siège, l’exclu est retiré comme un forfait', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await host.page.getByRole('button', { name: 'Options de Bob' }).click();
    const menu = host.page.getByRole('dialog', { name: 'Options de Bob' });
    await menu.getByRole('button', { name: 'Exclure' }).click();
    await menu.getByRole('button', { name: 'Exclure' }).click();

    await expect(guest.page.getByRole('heading', { name: 'Salon fermé' })).toBeVisible();
    await expect(
      host.page.getByRole('heading', { name: 'Alice remporte la partie !' }),
    ).toBeVisible();
  });
});

test.describe('retour au salon et revanche', () => {
  test.use({ seed: SEEDS.fastGame });

  test('en partie, l’hôte abandonne après confirmation : tout le monde est au salon', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);

    await expect(guest.page.getByRole('button', { name: 'Menu de la partie' })).toBeVisible();
    await host.page.getByRole('button', { name: 'Menu de la partie' }).click();
    await host.page.getByRole('button', { name: 'Retour au salon' }).click();
    await expect(host.page.getByRole('heading', { name: 'Abandonner la partie ?' })).toBeVisible();
    await expect(host.page.getByRole('heading', { name: 'Joueurs (2/6)' })).toBeHidden();
    await host.page.getByRole('dialog').getByRole('button', { name: 'Retour au salon' }).click();

    for (const player of [host, guest]) {
      await expect(player.page.getByRole('heading', { name: 'Joueurs (2/6)' })).toBeVisible();
    }
    // Plus de partie : on peut en relancer une
    await guest.page.getByRole('button', { name: 'Prêt' }).click();
    await host.page.getByRole('button', { name: 'Lancer la partie' }).click();
    await expect(host.cards).toHaveCount(7);
  });

  test('l’invité n’a pas de « Retour au salon » en cours de partie', async ({ lobby }) => {
    const { guest } = lobby;
    await startMatch(lobby);

    await guest.page.getByRole('button', { name: 'Menu de la partie' }).click();

    await expect(guest.page.getByRole('button', { name: 'Quitter la partie' })).toBeVisible();
    await expect(guest.page.getByRole('button', { name: 'Retour au salon' })).toHaveCount(0);
  });

  test('fin de partie : pas de relance automatique, l’hôte choisit la revanche, l’invité attend', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Manche unique');
    await startMatch(lobby);
    const finished = () =>
      guest.page.getByRole('heading', { name: /remporte la partie/ }).isVisible();
    await playUntil([guest, host], finished);
    // La cinématique de fin passe d'un clic ; l'écran des choix vient ensuite
    for (const player of [host, guest]) {
      await player.page.getByRole('button', { name: 'Passer' }).click();
    }

    await expect(guest.page.getByText('En attente de l’hôte')).toBeVisible();
    await expect(guest.page.getByRole('button', { name: 'Revanche' })).toHaveCount(0);
    await expect(host.page.getByRole('button', { name: 'Revanche' })).toBeVisible();
    await expect(host.page.getByRole('button', { name: 'Retour au salon' })).toBeVisible();
    // Rien ne démarre tout seul
    await host.page.waitForTimeout(1500);
    await expect(guest.page.getByRole('heading', { name: /remporte la partie/ })).toBeVisible();

    await host.page.getByRole('button', { name: 'Revanche' }).click();

    for (const player of [host, guest]) {
      await expect(player.cards).toHaveCount(7);
      await expect(player.page.getByRole('heading', { name: /remporte la partie/ })).toBeHidden();
    }
  });

  test('fin de partie : l’hôte peut aussi retourner au salon', async ({ lobby }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Manche unique');
    await startMatch(lobby);
    await playUntil([guest, host], async () =>
      guest.page.getByRole('heading', { name: /remporte la partie/ }).isVisible(),
    );

    await host.page.getByRole('button', { name: 'Passer' }).click();
    await host.page.getByRole('button', { name: 'Retour au salon' }).click();

    for (const player of [host, guest]) {
      await expect(player.page.getByRole('heading', { name: 'Joueurs (2/6)' })).toBeVisible();
    }
  });
});

test.describe('cinématique de fin de partie', () => {
  test.use({ seed: SEEDS.fastGame });

  test('le podium montre les deux joueurs avec leurs scores, se passe d’un clic puis laisse les choix', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Manche unique');
    await startMatch(lobby);
    await playUntil([guest, host], async () =>
      guest.page.getByRole('dialog', { name: 'Podium de la partie' }).isVisible(),
    );

    const podium = guest.page.getByRole('dialog', { name: 'Podium de la partie' });
    await expect(podium.getByRole('heading', { name: 'Bob remporte la partie !' })).toBeVisible();
    await expect(podium.locator('.place.rank-1 .name')).toHaveText('Bob');
    await expect(podium.locator('.place.rank-2 .name')).toHaveText('Alice');
    await expect(podium.locator('.place app-avatar')).toHaveCount(2);
    // Le score du gagnant est celui de la manche, une fois le défilement fini
    await expect(podium.locator('.place.rank-1 .score')).not.toHaveText('0 pts');
    await podium.getByRole('button', { name: 'Passer' }).click();

    await expect(podium).toBeHidden();
    await expect(guest.page.getByRole('dialog', { name: 'Fin de la partie' })).toBeVisible();
  });

  test('sans clic, elle s’efface d’elle-même', async ({ lobby }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Manche unique');
    await startMatch(lobby);
    await playUntil([guest, host], async () =>
      guest.page.getByRole('dialog', { name: 'Podium de la partie' }).isVisible(),
    );

    await expect(guest.page.getByRole('dialog', { name: 'Fin de la partie' })).toBeVisible({
      timeout: 8000,
    });
  });

  test('accessible (axe, WCAG 2.2 AA), le podium puis l’écran des choix', async ({ lobby }) => {
    const { host, guest } = lobby;
    await chooseSetting(host, 'Manche unique');
    await startMatch(lobby);
    await playUntil([guest, host], async () =>
      guest.page.getByRole('dialog', { name: 'Podium de la partie' }).isVisible(),
    );
    const tags = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];

    expect(
      (await new AxeBuilder({ page: guest.page }).withTags(tags).analyze()).violations,
    ).toEqual([]);
    await guest.page.getByRole('button', { name: 'Passer' }).click();
    await expect(guest.page.getByRole('dialog', { name: 'Fin de la partie' })).toBeVisible();
    expect(
      (await new AxeBuilder({ page: guest.page }).withTags(tags).analyze()).violations,
    ).toEqual([]);
  });
});

test.describe('chat', () => {
  test.use({ seed: SEEDS.quickGame });

  test('un message passe d’un joueur à l’autre, en texte brut, avec un compteur de non-lus', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await host.page.getByRole('button', { name: 'Chat' }).click();

    await host.page.getByLabel('Ton message').fill('Salut <b>Bob</b> !');
    await host.page.getByLabel('Ton message').press('Enter');

    // Bob n'a pas ouvert le panneau : un message non lu
    await expect(guest.page.getByLabel('1 messages non lus')).toBeVisible();
    await guest.page.getByRole('button', { name: /^Chat/ }).click();
    const line = guest.page.getByRole('list', { name: 'Messages' }).getByRole('listitem');
    await expect(line).toContainText('Alice');
    await expect(line).toContainText('Salut <b>Bob</b> !'); // affiché tel quel
    await expect(line.locator('b')).toHaveCount(0);
    await expect(guest.page.getByLabel('1 messages non lus')).toBeHidden();

    // Il répond, Alice le voit aussi
    await guest.page.getByLabel('Ton message').fill('Salut Alice');
    await guest.page.getByLabel('Ton message').press('Enter');
    await expect(
      host.page.getByRole('list', { name: 'Messages' }).getByText('Salut Alice'),
    ).toBeVisible();
  });

  test('le chat vit aussi à la table, et masquer un joueur coupe ses messages pour soi', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    await guest.page.getByRole('button', { name: /^Chat/ }).click();
    await host.page.getByRole('button', { name: /^Chat/ }).click();

    await host.page.getByLabel('Ton message').fill('Bonne chance');
    await host.page.getByLabel('Ton message').press('Enter');
    await expect(guest.page.getByText('Bonne chance')).toBeVisible();

    await guest.page.getByRole('button', { name: 'Options de Alice' }).click();
    await guest.page.getByRole('button', { name: 'Masquer ce joueur' }).click();
    await expect(guest.page.getByText('Bonne chance')).toBeHidden();
    await host.page.getByLabel('Ton message').fill('Tu me vois ?');
    await host.page.getByLabel('Ton message').press('Enter');
    await expect(host.page.getByText('Tu me vois ?')).toBeVisible();
    await expect(guest.page.getByText('Tu me vois ?')).toBeHidden();
  });

  test('un message trop long n’est pas accepté par le champ', async ({ lobby }) => {
    const { host } = lobby;
    await host.page.getByRole('button', { name: 'Chat' }).click();

    await host.page.getByLabel('Ton message').fill('x'.repeat(300));

    await expect(host.page.getByLabel('Ton message')).toHaveValue('x'.repeat(200));
  });
});

test.describe('avatar', () => {
  test('le choix est gardé avec le pseudo, d’une visite à l’autre', async ({ page, stack }) => {
    await page.goto(stack.url);
    await page.getByRole('radio', { name: 'Renard' }).click();
    await expect(page.getByRole('radio', { name: 'Renard' })).toHaveAttribute(
      'aria-checked',
      'true',
    );

    await page.reload();

    await expect(page.getByRole('radio', { name: 'Renard' })).toHaveAttribute(
      'aria-checked',
      'true',
    );
    expect(await page.evaluate(() => localStorage.getItem('uno.avatar'))).toBe('3');
  });
});

test.describe('accessibilité (axe, WCAG 2.2 AA)', () => {
  test.use({ seed: SEEDS.quickGame });
  const tags = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];

  test('l’accueil avec le choix d’avatar, le salon avec le chat ouvert et son menu de joueur', async ({
    lobby,
  }) => {
    const { host } = lobby;
    await host.page.getByRole('button', { name: 'Chat' }).click();
    await host.page.getByLabel('Ton message').fill('Bonjour');
    await host.page.getByLabel('Ton message').press('Enter');
    expect((await new AxeBuilder({ page: host.page }).withTags(tags).analyze()).violations).toEqual(
      [],
    );

    await host.page.getByRole('button', { name: 'Options de Bob' }).click();
    expect((await new AxeBuilder({ page: host.page }).withTags(tags).analyze()).violations).toEqual(
      [],
    );
  });

  test('la table avec le menu de partie et la fin de partie', async ({ lobby }) => {
    const { host } = lobby;
    await startMatch(lobby);

    await host.page.getByRole('button', { name: 'Menu de la partie' }).click();
    expect((await new AxeBuilder({ page: host.page }).withTags(tags).analyze()).violations).toEqual(
      [],
    );
  });

  test('l’accueil', async ({ page, stack }) => {
    await page.goto(stack.url);
    await expect(page.getByRole('radiogroup', { name: 'Ton avatar' })).toBeVisible();
    expect((await new AxeBuilder({ page }).withTags(tags).analyze()).violations).toEqual([]);
  });
});
