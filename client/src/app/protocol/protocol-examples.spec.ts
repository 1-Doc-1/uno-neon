import { readFileSync, readdirSync } from 'node:fs';
import { join, resolve } from 'node:path';

import Ajv2020, { type ValidateFunction } from 'ajv/dist/2020';

// Contract test: protocol/examples must agree with protocol/schema (the source of truth).
// The C++ server runs the same examples through its own codec (phase 2).
const protocolDir = resolve(process.cwd(), '../protocol');
const schemaDir = join(protocolDir, 'schema');
const validDir = join(protocolDir, 'examples/valid');
const invalidDir = join(protocolDir, 'examples/invalid');
const schemaBaseUri = 'https://uno-neon.invalid/protocol/v1/';

interface JsonSchema {
  oneOf?: unknown[];
  $defs?: Record<string, { properties?: Record<string, { const?: unknown }>; enum?: unknown[] }>;
}

function readJson(path: string): unknown {
  return JSON.parse(readFileSync(path, 'utf8'));
}

function readSchema(file: string): JsonSchema {
  return readJson(join(schemaDir, file)) as JsonSchema;
}

function jsonFiles(dir: string): string[] {
  return readdirSync(dir).filter((file) => file.endsWith('.json'));
}

function constValuesOf(schema: JsonSchema, discriminant: string): string[] {
  return Object.values(schema.$defs ?? {})
    .map((definition) => definition.properties?.[discriminant]?.const)
    .filter((value): value is string => typeof value === 'string');
}

function exampleName(file: string): string {
  return file.replace(/\.json$/, '');
}

function hasExampleFor(prefix: string, names: string[]): boolean {
  return names.some((name) => name === prefix || name.startsWith(`${prefix}.`));
}

function createValidators(): { client: ValidateFunction; server: ValidateFunction } {
  const ajv = new Ajv2020({ strict: true, allErrors: true });
  // json-schema-to-typescript extension, ignored by validation.
  ajv.addKeyword('tsType');
  for (const file of readdirSync(schemaDir)) {
    ajv.addSchema(readSchema(file) as object);
  }
  const client = ajv.getSchema(`${schemaBaseUri}client-message.schema.json`);
  const server = ajv.getSchema(`${schemaBaseUri}server-message.schema.json`);
  if (!client || !server) {
    throw new Error('Protocol schemas not found');
  }
  return { client, server };
}

describe('Protocol v1 examples', () => {
  const validators = createValidators();
  const validFiles = jsonFiles(validDir);
  // Every file, not only .json: an invalid example may not even be JSON.
  const invalidFiles = readdirSync(invalidDir);
  const validNames = validFiles.map(exampleName);

  it.each(validFiles)('accepts the valid example %s', (file) => {
    const validate = file.startsWith('client.') ? validators.client : validators.server;

    const isValid = validate(readJson(join(validDir, file)));

    expect(validate.errors ?? []).toEqual([]);
    expect(isValid).toBe(true);
  });

  it.each(invalidFiles)('rejects the invalid example %s', (file) => {
    const content = readFileSync(join(invalidDir, file), 'utf8');
    let message: unknown;
    try {
      message = JSON.parse(content);
    } catch {
      return; // Not even JSON: rejected as MALFORMED_MESSAGE.
    }

    expect(validators.client(message)).toBe(false);
  });

  it('names every invalid example after a known error code', () => {
    const errorCodes = readSchema('common.schema.json').$defs?.['ErrorCode']?.enum ?? [];

    for (const file of invalidFiles) {
      const expectedCode = file.split('.')[0];
      expect(errorCodes, file).toContain(expectedCode);
    }
  });

  it('has at least one valid example per client message type', () => {
    for (const type of constValuesOf(readSchema('client-message.schema.json'), 'type')) {
      expect(hasExampleFor(`client.${type}`, validNames), type).toBe(true);
    }
  });

  it('has at least one valid example per server message type', () => {
    for (const type of constValuesOf(readSchema('server-message.schema.json'), 'type')) {
      expect(hasExampleFor(`server.${type}`, validNames), type).toBe(true);
    }
  });

  it('uses every client event kind in at least one game.update example', () => {
    const usedKinds = new Set(
      validFiles
        .filter((file) => file.startsWith('server.game.update'))
        .flatMap((file) => {
          const message = readJson(join(validDir, file)) as {
            payload: { events: { kind: string }[] };
          };
          return message.payload.events.map((event) => event.kind);
        }),
    );

    for (const kind of constValuesOf(readSchema('client-event.schema.json'), 'kind')) {
      expect(usedKinds.has(kind), kind).toBe(true);
    }
  });
});
