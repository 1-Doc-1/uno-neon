import { expect, test } from '../support/fixtures.ts';
import { Player } from '../support/player.ts';

// Le journal estompe ses anciennes lignes par paliers d'opacité (table-view.scss). Chaque palier doit garder le
// contraste minimal de WCAG AA (4,5:1 pour ce corps de texte). axe ne sait pas déterminer le fond de cette zone
// (« recouvert par un autre élément », la table passe dessous) : on calcule donc le contraste nous-mêmes, avec la
// formule de WCAG, sur le fond le plus clair de la page (le centre du dégradé `--page-bg`), le pire cas.

test.use({ seed: 5 });

test('chaque palier d’estompage du journal garde un contraste AA', async ({ browser, stack }) => {
  const context = await browser.newContext({
    baseURL: stack.url,
    viewport: { width: 1280, height: 720 },
    locale: 'fr-FR',
    reducedMotion: 'reduce',
  });
  const player = new Player(await context.newPage(), 'Alice');
  const { page } = player;
  await page.goto('/');
  await page.getByLabel('Ton pseudo').fill('Alice');
  await page.getByRole('button', { name: /Jouer contre des bots/ }).click();
  await page.getByRole('button', { name: 'Jouer', exact: true }).click();
  await expect(player.handList).toBeVisible();

  const contrasts = await page.evaluate(() => {
    const list = document.querySelector('.journal');
    if (!list) return [];
    list.replaceChildren(
      ...[1, 2, 3, 4, 5].map((n) => {
        const line = document.createElement('li');
        line.textContent = `Ligne ${n} du journal`;
        return line;
      }),
    );
    const channels = (css: string): number[] => {
      const probe = document.createElement('span');
      probe.style.color = css;
      document.body.append(probe);
      const rgb = getComputedStyle(probe)
        .color.match(/[\d.]+/g)
        ?.map(Number) ?? [0, 0, 0];
      probe.remove();
      return rgb.slice(0, 3);
    };
    const luminance = ([r, g, b]: number[]): number => {
      const [lr, lg, lb] = [r, g, b].map((c) => {
        const s = (c ?? 0) / 255;
        return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
      });
      return 0.2126 * (lr ?? 0) + 0.7152 * (lg ?? 0) + 0.0722 * (lb ?? 0);
    };
    const pageBackground = getComputedStyle(document.documentElement).getPropertyValue('--page-bg');
    const lightest = [...pageBackground.matchAll(/#[0-9a-f]{6}/gi)]
      .map((match) => channels(match[0]))
      .reduce((a, b) => (luminance(a) >= luminance(b) ? a : b));
    return [...list.children].map((line) => {
      const style = getComputedStyle(line);
      const alpha = Number(style.opacity);
      const text = channels(style.color);
      const shown = text.map((c, i) => alpha * c + (1 - alpha) * (lightest[i] ?? 0));
      const [light, dark] = [luminance(shown), luminance(lightest)].sort((a, b) => b - a);
      return ((light ?? 0) + 0.05) / ((dark ?? 0) + 0.05);
    });
  });

  expect(contrasts, 'cinq paliers, du plus ancien au plus récent').toHaveLength(5);
  for (const [index, contrast] of contrasts.entries()) {
    expect(contrast, `ligne ${index + 1} : ${contrast.toFixed(2)}:1`).toBeGreaterThanOrEqual(4.5);
  }
  await context.close();
});
