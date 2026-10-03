import { expect, type Page } from '@playwright/test';

/**
 * Un joueur vu par son navigateur. Aucune attente fixe : tout passe par des états visibles (rôles, textes, libellés) ;
 * quand il faut attendre un changement, on sonde l'état jusqu'à ce qu'il change.
 */
export class Player {
  readonly page: Page;
  readonly name: string;

  constructor(page: Page, name: string) {
    this.page = page;
    this.name = name;
  }

  // ---- Ce qu'on voit ----
  get handList() {
    return this.page.getByRole('list', { name: 'Ta main' });
  }
  get cards() {
    return this.handList.getByRole('button');
  }
  /** Les cartes qu'on peut jouer (boutons actifs ; seul le tour du joueur en active). */
  get playableCards() {
    return this.page.locator('ul[aria-label="Ta main"] button:not([disabled])');
  }
  get myTurn() {
    return this.page.getByText('À toi de jouer');
  }
  get deck() {
    return this.page.locator('button.deck');
  }
  get colorPicker() {
    return this.page.getByRole('dialog', { name: 'Choisir une couleur' });
  }
  get acceptPenalty() {
    return this.page.getByRole('button', { name: /^Accepter, piocher/ });
  }
  get challengeButton() {
    return this.page.getByRole('button', { name: 'Contester' });
  }
  get unoButton() {
    return this.page.getByRole('button', { name: 'UNO !', exact: true });
  }
  /** Le code du salon affiché dans la barre du salon (et non le champ « Code du salon » de l'accueil). */
  get roomCode() {
    return this.page.locator('span[aria-label="Code du salon"]');
  }
  get journal() {
    return this.page.getByRole('list', { name: 'Journal de la partie' });
  }

  /** Le compteur de cartes d'un joueur (soi-même ou un adversaire), tel qu'affiché sur son siège. */
  cardCountOf(name: string) {
    const seat =
      name === this.name
        ? this.page.locator('app-my-badge')
        : this.page.locator('app-opponent-seat').filter({ hasText: name });
    return seat.getByLabel(/^\d+ cartes$/);
  }

  async handLabels(): Promise<string[]> {
    return (await this.cards.all()).length === 0
      ? []
      : this.cards.evaluateAll((buttons) =>
          buttons.map((button) => button.getAttribute('aria-label') ?? ''),
        );
  }

  /** La carte de la main nommée comme son libellé (« 5 rouge, jouable »), la première si elle est en double. */
  card(label: string | RegExp) {
    return this.handList.getByRole('button', { name: label }).first();
  }

  // ---- Ce qu'on fait ----
  async createRoom(): Promise<string> {
    await this.page.goto('/');
    await this.page.getByLabel('Ton pseudo').fill(this.name);
    await this.page.getByRole('button', { name: 'Créer un salon' }).click();
    const code = this.roomCode;
    await expect(code).toHaveText(/^[A-Z0-9]{6}$/);
    return (await code.textContent())?.trim() ?? '';
  }

  /** Rejoint par le lien d'invitation, comme un ami à qui on l'a envoyé. */
  async joinByLink(code: string): Promise<void> {
    await this.page.goto(`/r/${code}`);
    await this.page.getByLabel('Ton pseudo').fill(this.name);
    await this.page.getByRole('button', { name: 'Rejoindre' }).click();
    await expect(this.roomCode).toHaveText(code);
  }

  /** Une photographie de ce qui peut changer après un coup : la main, le tour, les compteurs, les fenêtres. */
  async snapshot(): Promise<string> {
    return this.page.evaluate(() => {
      const text = (selector: string): string[] =>
        [...document.querySelectorAll(selector)].map(
          (element) => element.textContent?.trim() ?? '',
        );
      return JSON.stringify({
        hand: [...document.querySelectorAll('ul[aria-label="Ta main"] button')].map(
          (button) => button.getAttribute('aria-label') ?? '',
        ),
        turn: text('p.turn'),
        counts: text('.count'),
        dialogs: [...document.querySelectorAll('dialog[open]')].map((dialog) =>
          dialog.getAttribute('aria-label'),
        ),
        deck: document.querySelector('button.deck')?.getAttribute('aria-label') ?? '',
      });
    });
  }

  /** Vrai si la politique de jeu a quelque chose à faire maintenant (sans rien faire). */
  async canAct(): Promise<boolean> {
    if (await this.acceptPenalty.isVisible()) {
      return true;
    }
    if (await this.colorPicker.isVisible()) {
      return true;
    }
    return (await this.myTurn.isVisible()) && (await this.playableCards.count()) > 0;
  }

  /**
   * Un coup de la politique des tests (la même que `choosePlay` du chercheur de graines) : accepter une pénalité,
   * choisir rouge, sinon jouer la première carte jouable de la main. Rend `false` s'il n'y a rien à faire ; quand rien
   * ne se joue, le serveur pioche de lui-même (pioche guidée). Attend que l'écran ait changé avant de rendre la main.
   */
  async actNow(): Promise<boolean> {
    if (!(await this.canAct())) {
      return false;
    }
    const before = await this.snapshot();
    if (await this.acceptPenalty.isVisible()) {
      await this.acceptPenalty.click();
    } else if (await this.colorPicker.isVisible()) {
      await this.colorPicker.getByRole('button', { name: 'Rouge' }).click();
    } else {
      await this.playableCards.first().click();
    }
    await expect
      .poll(() => this.snapshot(), { message: `l'écran de ${this.name} n'a pas réagi` })
      .not.toBe(before);
    return true;
  }
}

/**
 * Fait jouer tous les joueurs selon la politique jusqu'à ce que `done` soit vrai. Chaque tour de boucle regarde `done`
 * d'abord, puis laisse chaque joueur faire un coup ; s'il n'y a rien à faire, on attend qu'un joueur ait quelque chose à
 * faire ou que `done` devienne vrai.
 */
export async function playUntil(
  players: readonly Player[],
  done: () => Promise<boolean>,
): Promise<void> {
  for (;;) {
    if (await done()) {
      return;
    }
    let acted = false;
    for (const player of players) {
      if (await done()) {
        return;
      }
      acted = (await player.actNow()) || acted;
    }
    if (!acted) {
      await expect
        .poll(async () => (await done()) || (await anyCanAct(players)), {
          message: 'plus personne ne peut jouer',
        })
        .toBe(true);
    }
  }
}

async function anyCanAct(players: readonly Player[]): Promise<boolean> {
  for (const player of players) {
    if (await player.canAct()) {
      return true;
    }
  }
  return false;
}
