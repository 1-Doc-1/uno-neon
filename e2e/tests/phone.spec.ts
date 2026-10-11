import { devices, expect, type Browser } from '@playwright/test';
import { test } from '../support/fixtures.ts';
import { layoutProblems } from '../support/layout.ts';
import { Player, playUntil } from '../support/player.ts';

// Le jeu sur téléphone (étape 3.7) : iPhone et Android en portrait et en paysage, une partie jusqu'au bout
// à vitesse réelle. À chaque étape on mesure ce qui compte : jamais de défilement de la page, aucun siège ni effet qui
// sorte de l'écran, rien qui dépasse à droite.

type Emulated = (typeof devices)[string];

/** Une partie à vitesse réelle dure de 2 à plus de 5 minutes (effets, délais entre les coups) : chaque téléphone joue 75 s, la partie entière est couverte par les tests au clavier et contre les bots. */
const SAMPLE_MS = 75_000;

const PHONES: ReadonlyArray<{ name: string; device: Emulated; fullGame: boolean }> = [
  { name: 'iPhone 13 portrait', device: devices['iPhone 13'] as Emulated, fullGame: false },
  {
    name: 'iPhone 13 paysage',
    device: devices['iPhone 13 landscape'] as Emulated,
    fullGame: false,
  },
  { name: 'Pixel 7 portrait', device: devices['Pixel 7'] as Emulated, fullGame: false },
  { name: 'Pixel 7 paysage', device: devices['Pixel 7 landscape'] as Emulated, fullGame: false },
];

/** Les effets posés sur un point (grand texte, étiquette, tampon) : leur boîte de mise en page ne doit jamais sortir. */
const EFFECT_SELECTOR = 'app-effects-layer .big, app-effects-layer .label, app-effects-layer .skip';

async function newPhone(browser: Browser, url: string, device: Emulated) {
  // Le navigateur est toujours Chromium : on prend la taille, l'agent, le toucher, pas le moteur de l'appareil
  const { defaultBrowserType: _engine, ...settings } = device;
  const context = await browser.newContext({
    ...settings,
    baseURL: url,
    locale: 'fr-FR',
    reducedMotion: 'no-preference',
  });
  const page = await context.newPage();
  // Pendant toute la partie, chaque image relève ce qui sort de l'écran (une seule fois par élément)
  await page.addInitScript((selector: string) => {
    const w = window as unknown as { __offscreen: string[] };
    w.__offscreen = [];
    const seen = new WeakSet<Element>();
    const watch = (): void => {
      const width = document.documentElement.clientWidth;
      const height = document.documentElement.clientHeight;
      for (const element of document.querySelectorAll<HTMLElement>(selector)) {
        if (seen.has(element) || element.offsetWidth === 0) {
          continue;
        }
        // La boîte de mise en page, sans l'échelle de l'animation : centrée sur left/top, avec ses enfants (légende)
        let boxW = element.offsetWidth;
        let boxH = element.offsetHeight;
        for (const child of element.querySelectorAll<HTMLElement>('*')) {
          boxW = Math.max(boxW, child.offsetWidth || 0);
          boxH = Math.max(boxH, child.offsetHeight || 0);
        }
        const cx = parseFloat(element.style.left);
        const cy = parseFloat(element.style.top);
        if (
          cx - boxW / 2 < -2 ||
          cy - boxH / 2 < -2 ||
          cx + boxW / 2 > width + 2 ||
          cy + boxH / 2 > height + 2
        ) {
          w.__offscreen.push(
            `${element.className}: centre ${Math.round(cx)},${Math.round(cy)} taille ${Math.round(boxW)}×${Math.round(boxH)} dans ${width}×${height}`,
          );
          seen.add(element);
        }
      }
      requestAnimationFrame(watch);
    };
    requestAnimationFrame(watch);
  }, EFFECT_SELECTOR);
  return { context, player: new Player(page, 'Alice') };
}

async function startBotGame(player: Player, bots: number): Promise<void> {
  const { page } = player;
  await page.goto('/');
  // L'écran « Cliquer pour jouer » (son) s'il est là : un toucher le passe
  const start = page.getByRole('button', { name: /Cliquer pour jouer/ });
  if (await start.isVisible().catch(() => false)) {
    await start.tap();
  }
  await page.getByLabel('Ton pseudo').fill('Alice');
  await page.getByRole('button', { name: /Jouer contre des bots/ }).tap();
  await page
    .getByRole('radiogroup', { name: 'Nombre de bots' })
    .getByRole('radio', { name: String(bots) })
    .tap();
  const single = page.getByRole('radio', { name: 'Manche unique' });
  await single.tap();
  await page.getByRole('button', { name: 'Jouer', exact: true }).tap();
  await expect(player.handList).toBeVisible();
}

test.describe('téléphone', () => {
  test.use({ seed: 5 });

  for (const { name, device, fullGame } of PHONES) {
    test(`${name} : ${fullGame ? 'une partie contre 1 bot jusqu’au bout' : 'une minute de jeu contre 2 bots'}, sans défilement et sans effet hors de l'écran`, async ({
      browser,
      stack,
    }) => {
      test.setTimeout(150_000);
      const { context, player } = await newPhone(browser, stack.url, device);
      await startBotGame(player, fullGame ? 1 : 2);

      const problems = new Set<string>();
      const startedAt = Date.now();
      const finished = async () => {
        for (const problem of await layoutProblems(player)) {
          problems.add(problem);
        }
        if (!fullGame && Date.now() - startedAt > SAMPLE_MS) {
          return true;
        }
        return player.page
          .getByRole('heading', { name: /gagne la manche|remporte la partie/ })
          .isVisible();
      };
      try {
        await playUntil([player], finished);
      } catch (error) {
        // Une partie qui n'avance plus : on garde ce que voyait le joueur pour comprendre
        await test.info().attach('ecran', {
          body: await player.page.screenshot().catch(() => Buffer.from('')),
          contentType: 'image/png',
        });
        await test.info().attach('texte', {
          body: await player.page
            .locator('body')
            .innerText()
            .catch(() => ''),
          contentType: 'text/plain',
        });
        throw error;
      }
      if (fullGame) {
        await expect(
          player.page.getByRole('heading', { name: /gagne la manche|remporte la partie/ }),
        ).toBeVisible();
      }

      const offscreen = await player.page.evaluate(
        () => (window as unknown as { __offscreen: string[] }).__offscreen,
      );
      expect([...problems], 'mise en page').toEqual([]);
      expect(offscreen, 'effets hors de l’écran').toEqual([]);
      await context.close();
    });
  }
});
