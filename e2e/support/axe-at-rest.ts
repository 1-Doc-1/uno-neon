import { AxeBuilder } from '@axe-core/playwright';
import type { Page } from '@playwright/test';

const TAGS = ['wcag2a', 'wcag2aa', 'wcag22aa'];
const ATTEMPTS = 20;

/**
 * Compte les éléments d'effet que la couche des effets a reçus depuis l'installation de l'observateur. Si ce compte
 * bouge pendant une analyse, un effet est apparu en cours de route et la mesure est à refaire.
 */
async function watchEffects(page: Page): Promise<void> {
  await page.evaluate(() => {
    const w = window as unknown as { __effectsSeen?: number };
    if (w.__effectsSeen !== undefined) return;
    w.__effectsSeen = 0;
    const layer = document.querySelector('app-effects-layer');
    if (!layer) return;
    new MutationObserver((records) => {
      for (const record of records) {
        w.__effectsSeen = (w.__effectsSeen ?? 0) + record.addedNodes.length;
      }
    }).observe(layer, { childList: true });
  });
}

/** Aucun effet affiché et aucune animation finie en cours (les boucles infinies, comme le souffle du paquet, restent). */
function atRest(): boolean {
  const layer = document.querySelector('app-effects-layer');
  const effects = [...(layer?.children ?? [])].filter(
    (child) => child.tagName !== 'APP-PARTICLE-CANVAS',
  );
  const running = document
    .getAnimations()
    .some(
      (animation) =>
        animation.playState === 'running' &&
        animation.effect?.getComputedTiming().iterations !== Infinity,
    );
  return effects.length === 0 && !running;
}

/**
 * Analyse axe (WCAG 2.2 AA) à un moment stable : ni effet, ni fondu en cours, avant comme après l'analyse. Rien n'est
 * exclu de la page. Renvoie les violations (une ligne chacune).
 */
export async function axeViolationsAtRest(page: Page): Promise<string[]> {
  await watchEffects(page);
  for (let attempt = 0; attempt < ATTEMPTS; attempt++) {
    await page.waitForFunction(atRest, undefined, { timeout: 15_000 });
    const before = await page.evaluate(
      () => (window as unknown as { __effectsSeen: number }).__effectsSeen,
    );
    const result = await new AxeBuilder({ page }).withTags(TAGS).analyze();
    const after = await page.evaluate(() => ({
      seen: (window as unknown as { __effectsSeen: number }).__effectsSeen,
      rest: (document.querySelector('app-effects-layer')?.children.length ?? 0) <= 1,
    }));
    if (after.seen === before && after.rest) {
      return result.violations.flatMap((violation) =>
        violation.nodes.map(
          (node) => `${violation.id}: ${node.target.join(' ')} ${node.any[0]?.message ?? ''}`,
        ),
      );
    }
  }
  throw new Error(`Aucun moment stable en ${ATTEMPTS} essais : la table n'est jamais au repos`);
}
