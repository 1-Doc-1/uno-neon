import { AxeBuilder } from '@axe-core/playwright';
import { expect, test } from '../support/fixtures.ts';
import { startMatch } from '../support/flow.ts';
import { SEEDS } from '../support/seeds.ts';

// La roue de choix de couleur : un quart de disque par couleur, qui se déploie depuis la défausse, sans rien qui déborde.

const SIZES = [
  { width: 375, height: 700 },
  { width: 768, height: 1024 },
  { width: 1280, height: 720 },
  { width: 1920, height: 1080 },
];

test.describe('roue de couleur', () => {
  test.use({ seed: SEEDS.jokerInHand });

  for (const size of SIZES) {
    test(`rien ne déborde de la roue ni de l’écran à ${size.width}×${size.height}`, async ({
      lobby,
    }) => {
      const { guest } = lobby;
      await guest.page.setViewportSize(size);
      await startMatch(lobby);

      await guest.card('Joker, jouable').click();
      await expect(guest.colorPicker).toBeVisible();

      const boxes = await guest.page.evaluate(() => {
        const rect = (element: Element) => {
          const { left, top, right, bottom } = element.getBoundingClientRect();
          return { left, top, right, bottom };
        };
        const wheel = document.querySelector('app-color-picker .wheel');
        return {
          wheel: wheel ? rect(wheel) : null,
          wedges: [...document.querySelectorAll('app-color-picker .wedge')].map(rect),
          contents: [...document.querySelectorAll('app-color-picker .wedge .content')].map(rect),
          viewport: { width: innerWidth, height: innerHeight },
        };
      });
      expect(boxes.wheel).not.toBeNull();
      const wheel = boxes.wheel as NonNullable<typeof boxes.wheel>;
      expect(boxes.wedges).toHaveLength(4);
      // La roue est entière dans l'écran, chaque quart dans la roue, chaque libellé dans son quart
      expect(wheel.left).toBeGreaterThanOrEqual(0);
      expect(wheel.top).toBeGreaterThanOrEqual(0);
      expect(wheel.right).toBeLessThanOrEqual(boxes.viewport.width);
      expect(wheel.bottom).toBeLessThanOrEqual(boxes.viewport.height);
      for (const [index, wedge] of boxes.wedges.entries()) {
        expect(wedge.left, `quart ${index}`).toBeGreaterThanOrEqual(wheel.left - 1);
        expect(wedge.top, `quart ${index}`).toBeGreaterThanOrEqual(wheel.top - 1);
        expect(wedge.right, `quart ${index}`).toBeLessThanOrEqual(wheel.right + 1);
        expect(wedge.bottom, `quart ${index}`).toBeLessThanOrEqual(wheel.bottom + 1);
        const content = boxes.contents[index] as NonNullable<(typeof boxes.contents)[number]>;
        expect(content.left, `libellé ${index}`).toBeGreaterThanOrEqual(wedge.left - 1);
        expect(content.top, `libellé ${index}`).toBeGreaterThanOrEqual(wedge.top - 1);
        expect(content.right, `libellé ${index}`).toBeLessThanOrEqual(wedge.right + 1);
        expect(content.bottom, `libellé ${index}`).toBeLessThanOrEqual(wedge.bottom + 1);
      }
    });
  }

  test('au clavier : le focus est sur la roue, Tab fait le tour, les touches 1 à 4 et Échap marchent', async ({
    lobby,
  }) => {
    const { host, guest } = lobby;
    await startMatch(lobby);
    await guest.card('Joker, jouable').click();
    await expect(guest.colorPicker.getByRole('button', { name: 'Rouge' })).toBeFocused();

    await guest.page.keyboard.press('Tab');
    await expect(guest.colorPicker.getByRole('button', { name: 'Jaune' })).toBeFocused();
    await guest.page.keyboard.press('Escape');
    await expect(guest.colorPicker).toBeHidden();

    await guest.card('Joker, jouable').click();
    await expect(guest.colorPicker).toBeVisible();
    await guest.page.keyboard.press('4');
    await expect(guest.colorPicker).toBeHidden();
    for (const player of [host, guest]) {
      await expect(player.page.locator('app-piles .color')).toContainText('Bleu');
    }
  });

  test('accessible (axe, WCAG 2.2 AA)', async ({ lobby }) => {
    const { guest } = lobby;
    await startMatch(lobby);
    await guest.card('Joker, jouable').click();
    await expect(guest.colorPicker).toBeVisible();

    const result = await new AxeBuilder({ page: guest.page })
      .withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'])
      .analyze();

    expect(result.violations).toEqual([]);
  });
});

test.describe('roue de couleur, mouvement réel', () => {
  test.use({ seed: SEEDS.jokerInHand, motion: 'no-preference' });

  test('la couleur choisie grossit avec des étincelles avant de partir', async ({ lobby }) => {
    const { guest } = lobby;
    await startMatch(lobby);
    await guest.card('Joker, jouable').click();

    await guest.colorPicker.getByRole('button', { name: 'Bleu' }).click();

    await expect(guest.colorPicker.locator('.wedge.chosen')).toBeVisible();
    await expect(guest.colorPicker.locator('.sparks i')).not.toHaveCount(0);
    await expect(guest.colorPicker).toBeHidden();
    await expect(guest.page.locator('app-piles .color')).toContainText('Bleu');
  });
});
