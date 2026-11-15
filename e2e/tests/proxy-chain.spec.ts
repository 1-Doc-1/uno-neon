import { type ChildProcess, spawn, spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { connect, createServer, type Socket } from 'node:net';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { expect, test } from '@playwright/test';

// La chaîne de relais du déploiement (ADR 0044) : « Traefik » (ce test) → le Caddy de production
// (deploy/Caddyfile.production) → le serveur de jeu. Le serveur ne croit que l'en-tête posé par Caddy, et Caddy ne croit
// que Traefik. Les limites par adresse (20 connexions) servent de témoin : elles ne comptent la bonne adresse que si
// toute la chaîne de confiance est correcte. Ces tests exigent Caddy (`caddy` dans le PATH, ou CADDY_BIN).

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const SERVER_BINARY =
  process.env['UNO_SERVER_BIN'] ??
  join(root, 'server/build/e2e', process.platform === 'win32' ? 'uno_server.exe' : 'uno_server');
const CADDY = process.env['CADDY_BIN'] ?? 'caddy';
const PUBLIC_ORIGIN = 'https://noc.example.test';
const PASSWORD = 'soiree-entre-amis';

async function freePort(): Promise<number> {
  const probe = createServer();
  await new Promise<void>((done) => probe.listen(0, '127.0.0.1', done));
  const address = probe.address();
  await new Promise<void>((done) => probe.close(() => done()));
  if (address === null || typeof address === 'string') {
    throw new Error('Aucun port libre');
  }
  return address.port;
}

function hashPassword(password: string): string {
  const result = spawnSync(CADDY, ['hash-password'], { input: `${password}\n`, encoding: 'utf8' });
  if (result.status !== 0 || !result.stdout.startsWith('$2')) {
    throw new Error(`Caddy introuvable ou en échec : ${result.error?.message ?? result.stderr}`);
  }
  return result.stdout.trim();
}

interface Chain {
  readonly webPort: number;
  stop(): void;
}

/** Serveur de jeu + Caddy de production, le tout sur 127.0.0.1 ; « Traefik » est le client de ce test (127.0.0.1). */
async function startChain(sitePasswordHash?: string): Promise<Chain> {
  const gamePort = await freePort();
  const webPort = await freePort();
  const work = mkdtempSync(join(tmpdir(), 'noc-chain-')).replaceAll('\\', '/');
  const caddyfile = readFileSync(join(root, 'deploy/Caddyfile.production'), 'utf8')
    .replace('http://:8080', `http://:${webPort}`)
    .replaceAll('server:9001', `127.0.0.1:${gamePort}`)
    .replace('/etc/caddy/csp.caddy', `${work}/csp.caddy`)
    .replace('root * /srv', `root * ${work}`);
  writeFileSync(`${work}/Caddyfile`, caddyfile);
  writeFileSync(`${work}/csp.caddy`, `header Content-Security-Policy "default-src 'self'"\n`);
  writeFileSync(`${work}/index.html`, '<app-root></app-root>');

  const server = spawn(SERVER_BINARY, [], {
    stdio: 'ignore',
    env: {
      ...process.env,
      UNO_PORT: String(gamePort),
      UNO_BIND_ADDRESS: '127.0.0.1',
      UNO_ALLOWED_ORIGINS: PUBLIC_ORIGIN,
      // Comme dans docker-compose.yml : le serveur ne fait confiance qu'à l'adresse de Caddy
      UNO_TRUSTED_PROXY_ADDRESSES: '127.0.0.1',
      UNO_LOG_LEVEL: 'error',
    },
  });
  const caddy: ChildProcess = spawn(
    CADDY,
    ['run', '--config', `${work}/Caddyfile`, '--adapter', 'caddyfile'],
    {
      stdio: 'ignore',
      env: {
        ...process.env,
        // « Traefik » (le client de ce test) vient de 127.0.0.1 : c'est lui, et lui seul, que Caddy croit
        NOC_TRUSTED_PROXIES: '127.0.0.1/32',
        ...(sitePasswordHash === undefined ? {} : { SITE_PASSWORD_HASH: sitePasswordHash }),
      },
    },
  );
  const deadline = Date.now() + 15_000;
  for (;;) {
    try {
      if ((await fetch(`http://127.0.0.1:${webPort}/sante`)).status === 200) {
        break;
      }
    } catch {
      // pas encore à l'écoute
    }
    if (Date.now() > deadline) {
      server.kill();
      caddy.kill();
      throw new Error('La chaîne ne répond pas sur /sante.');
    }
    await new Promise((done) => setTimeout(done, 100));
  }
  return {
    webPort,
    stop() {
      server.kill();
      caddy.kill();
    },
  };
}

interface Reply {
  readonly status: string;
  readonly socket: Socket;
}

/** Une requête HTTP brute (« Traefik » parle à Caddy), avec les en-têtes voulus ; la socket reste ouverte. */
function request(port: number, path: string, headers: string[]): Promise<Reply> {
  return new Promise((resolveReply) => {
    const socket = connect(port, '127.0.0.1', () => {
      const lines = [`GET ${path} HTTP/1.1`, 'Host: noc.example.test', ...headers];
      socket.write(`${lines.join('\r\n')}\r\n\r\n`);
    });
    socket.setTimeout(5000, () => {
      socket.destroy();
      resolveReply({ status: 'timeout', socket });
    });
    socket.once('data', (chunk: Buffer) =>
      resolveReply({ status: chunk.toString('latin1').split('\r\n')[0] ?? '', socket }),
    );
    socket.once('error', () => resolveReply({ status: 'error', socket }));
  });
}

const websocket = (port: number, forwardedFor?: string, authorization?: string) =>
  request(port, '/ws', [
    `Origin: ${PUBLIC_ORIGIN}`,
    'Upgrade: websocket',
    'Connection: Upgrade',
    'Sec-WebSocket-Version: 13',
    'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
    ...(forwardedFor === undefined ? [] : [`X-Forwarded-For: ${forwardedFor}`]),
    ...(authorization === undefined ? [] : [`Authorization: ${authorization}`]),
  ]);

test.describe('Chaîne Traefik → Caddy → serveur, site ouvert', () => {
  let chain: Chain;
  test.beforeAll(async () => {
    chain = await startChain();
  });
  test.afterAll(() => chain.stop());

  test('chaque visiteur est compté sous sa propre adresse, jamais sous celle qu’il écrit', async () => {
    // 25 visiteurs, chacun écrivant « 9.9.9.9 » en tête de son X-Forwarded-For : si cette adresse était crue, ou si le
    // serveur voyait tout le monde sous l'adresse de Caddy, la limite de 20 connexions par adresse en refuserait.
    const sockets: Socket[] = [];
    let accepted = 0;
    for (let visitor = 1; visitor <= 25; visitor += 1) {
      const reply = await websocket(chain.webPort, `9.9.9.9, 203.0.113.${visitor}`);
      sockets.push(reply.socket);
      accepted += reply.status.includes('101') ? 1 : 0;
    }
    sockets.forEach((socket) => socket.destroy());
    expect(accepted).toBe(25);
  });

  test('un seul visiteur reste soumis à la limite de 20 connexions par adresse', async () => {
    const sockets: Socket[] = [];
    let accepted = 0;
    let refused = 0;
    for (let attempt = 1; attempt <= 25; attempt += 1) {
      const reply = await websocket(chain.webPort, '198.51.100.7');
      sockets.push(reply.socket);
      accepted += reply.status.includes('101') ? 1 : 0;
      refused += reply.status.includes('503') ? 1 : 0;
    }
    sockets.forEach((socket) => socket.destroy());
    expect({ accepted, refused }).toEqual({ accepted: 20, refused: 5 });
  });

  test('/sante répond ok sans rien révéler, /health est fermé au public', async () => {
    const sante = await fetch(`http://127.0.0.1:${chain.webPort}/sante`);
    expect(sante.status).toBe(200);
    expect(await sante.text()).toBe('ok');
    const health = await fetch(`http://127.0.0.1:${chain.webPort}/health`);
    expect(health.status).toBe(404);
  });

  test('une Origin étrangère est refusée', async () => {
    const reply = await request(chain.webPort, '/ws', [
      'Origin: https://evil.example',
      'Upgrade: websocket',
      'Connection: Upgrade',
      'Sec-WebSocket-Version: 13',
      'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
    ]);
    reply.socket.destroy();
    expect(reply.status).toContain('403');
  });
});

test.describe('Chaîne Traefik → Caddy → serveur, site protégé', () => {
  let chain: Chain;
  test.beforeAll(async () => {
    chain = await startChain(hashPassword(PASSWORD));
  });
  test.afterAll(() => chain.stop());

  test('sans mot de passe : 401 sur la page et sur /ws ; /sante reste ouverte', async () => {
    const page = await fetch(`http://127.0.0.1:${chain.webPort}/`);
    expect(page.status).toBe(401);
    const reply = await websocket(chain.webPort);
    reply.socket.destroy();
    expect(reply.status).toContain('401');
    expect((await fetch(`http://127.0.0.1:${chain.webPort}/sante`)).status).toBe(200);
  });

  test('avec le mot de passe : la page répond et le WebSocket s’ouvre', async () => {
    const credentials = `Basic ${Buffer.from(`noc:${PASSWORD}`).toString('base64')}`;
    const page = await fetch(`http://127.0.0.1:${chain.webPort}/`, {
      headers: { Authorization: credentials },
    });
    expect(page.status).toBe(200);
    const reply = await websocket(chain.webPort, '203.0.113.5', credentials);
    reply.socket.destroy();
    expect(reply.status).toContain('101');
  });
});
