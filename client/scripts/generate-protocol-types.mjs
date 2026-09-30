// Generates the TypeScript types of the protocol from protocol/schema (the source of truth).
// Usage: npm run protocol:gen
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { compileFromFile } from 'json-schema-to-typescript';
import prettier from 'prettier';

const clientDir = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const schemaDir = resolve(clientDir, '../protocol/schema');
const outputFile = resolve(clientDir, 'src/app/protocol/generated/protocol.ts');

const banner = `/*
 * GÉNÉRÉ AUTOMATIQUEMENT depuis protocol/schema par \`npm run protocol:gen\`.
 * Ne pas modifier à la main : modifier le schéma JSON puis relancer la génération.
 */`;

const types = await compileFromFile(resolve(schemaDir, 'protocol.schema.json'), {
  cwd: schemaDir,
  bannerComment: banner,
  additionalProperties: false,
  // Keep `T[]` for bounded arrays instead of a union of tuples of every length.
  maxItems: -1,
  strictIndexSignatures: true,
  format: false,
});

const prettierConfig = await prettier.resolveConfig(outputFile);
const formatted = await prettier.format(types, { ...prettierConfig, filepath: outputFile });

await mkdir(dirname(outputFile), { recursive: true });
await writeFile(outputFile, formatted);
console.log(`Types du protocole générés : ${outputFile}`);
