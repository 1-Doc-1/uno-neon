import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: 'tests',
  // Chaque scénario doit tenir en moins de 30 s (une partie jouée en quelques dizaines de coups au plus)
  timeout: 30_000,
  expect: { timeout: 10_000 },
  fullyParallel: true,
  workers: process.env['CI'] ? 2 : 3,
  forbidOnly: !!process.env['CI'],
  retries: 0,
  reporter: process.env['CI'] ? [['list'], ['html', { open: 'never' }]] : [['list']],
  use: {
    ...devices['Desktop Chrome'],
    viewport: { width: 1280, height: 720 },
    locale: 'fr-FR',
    // Animations réduites : les effets deviennent de courts fondus, aucun test n'attend une animation
    reducedMotion: 'reduce',
    trace: 'retain-on-failure',
  },
});
