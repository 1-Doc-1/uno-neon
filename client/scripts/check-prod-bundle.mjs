// Vérifie que le build de production ne contient aucune page de développement ni aucune fixture.
// Usage : npm run build && node scripts/check-prod-bundle.mjs   (ou : npm run test:bundle)
import { readdir, readFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../dist/client/browser');

// Des chaînes qui n'existent que dans les pages /dev et leurs scénarios
const FORBIDDEN = [
  'dev/table',
  'scenarioView',
  'players-6',
  'uno-window',
  'two-windows',
  'must-declare',
  'full-table',
  'Démo des animations',
  'Loïc pose un 8 rouge',
];
// Une chaîne de l'application elle-même : prouve que l'on a bien lu le bundle de l'application
const EXPECTED = 'session.hello';

const files = (await readdir(root)).filter((name) => name.endsWith('.js'));
if (files.length === 0) {
  console.error(`Aucun fichier .js dans ${root} : lancer d'abord "npm run build".`);
  process.exit(2);
}

let sawApplication = false;
const leaks = [];
for (const file of files) {
  const text = await readFile(join(root, file), 'utf8');
  sawApplication ||= text.includes(EXPECTED);
  for (const marker of FORBIDDEN) {
    if (text.includes(marker)) {
      leaks.push(`${file} contient « ${marker} »`);
    }
  }
}

if (!sawApplication) {
  console.error(
    `Le bundle de l'application (« ${EXPECTED} ») est introuvable : la vérification ne prouverait rien.`,
  );
  process.exit(2);
}
if (leaks.length > 0) {
  console.error('Des pages ou fixtures de développement sont dans le build de production :');
  for (const leak of leaks) {
    console.error(`  - ${leak}`);
  }
  process.exit(1);
}
console.log(`OK : ${files.length} fichiers JavaScript, aucune page de développement.`);
