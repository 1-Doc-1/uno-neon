import { type ChildProcess, spawn } from 'node:child_process';
import { existsSync, readFileSync, statSync } from 'node:fs';
import { createServer, type IncomingMessage, type Server } from 'node:http';
import { connect, createServer as createTcpServer, type Socket } from 'node:net';
import { dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const CLIENT_DIST = join(root, 'client/dist/client/browser');
const SERVER_BINARY =
  process.env['UNO_SERVER_BIN'] ??
  join(root, 'server/build/e2e', process.platform === 'win32' ? 'uno_server.exe' : 'uno_server');

const MIME: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.svg': 'image/svg+xml',
  '.woff2': 'font/woff2',
  '.woff': 'font/woff',
  '.ico': 'image/x-icon',
};

export interface StackOptions {
  /** Graine du serveur (UNO_TEST_SEED) : même graine, mêmes cartes, si la partie se déroule dans le même ordre. */
  readonly seed: number;
  /** Délai de grâce avant le forfait d'un joueur déconnecté (UNO_TEST_RECONNECT_GRACE_MS). */
  readonly reconnectGraceMs?: number;
}

export interface Stack {
  /** Adresse du client ; `/ws` y est relayé vers le serveur de jeu de cette pile. */
  readonly url: string;
  stop(): Promise<void>;
}

/** Un port libre : on ouvre un serveur sur le port 0, on lit le port choisi, on le referme. */
async function freePort(): Promise<number> {
  const probe = createTcpServer();
  await new Promise<void>((done) => probe.listen(0, '127.0.0.1', done));
  const address = probe.address();
  await new Promise<void>((done) => probe.close(() => done()));
  if (address === null || typeof address === 'string') {
    throw new Error('Aucun port libre');
  }
  return address.port;
}

function serveFile(urlPath: string): { body: Buffer; type: string } {
  const requested = normalize(join(CLIENT_DIST, decodeURIComponent(urlPath.split('?')[0] ?? '/')));
  const inside = requested === CLIENT_DIST || requested.startsWith(CLIENT_DIST + sep);
  const file =
    inside && existsSync(requested) && statSync(requested).isFile()
      ? requested
      : join(CLIENT_DIST, 'index.html');
  return {
    body: readFileSync(file),
    type: MIME[extname(file)] ?? 'application/octet-stream',
  };
}

/** Le client construit, servi tel quel (repli sur index.html pour les routes), avec `/ws` relayé vers le serveur de jeu. */
async function startWebServer(
  gamePort: () => number,
  tunnels: Set<Socket>,
): Promise<{ server: Server; port: number }> {
  const server = createServer((request, response) => {
    const { body, type } = serveFile(request.url ?? '/');
    response.writeHead(200, { 'content-type': type, 'cache-control': 'no-store' }).end(body);
  });
  server.on('upgrade', (request: IncomingMessage, client: Socket, head: Buffer) => {
    const upstream = connect(gamePort(), '127.0.0.1', () => {
      const lines = [`${request.method} ${request.url} HTTP/1.1`];
      for (let index = 0; index < request.rawHeaders.length; index += 2) {
        lines.push(`${request.rawHeaders[index]}: ${request.rawHeaders[index + 1]}`);
      }
      upstream.write(`${lines.join('\r\n')}\r\n\r\n`);
      upstream.write(head);
      client.pipe(upstream).pipe(client);
    });
    // Les sockets « upgradées » échappent à closeAllConnections() : on les suit pour les détruire à l'arrêt
    for (const socket of [client, upstream]) {
      tunnels.add(socket);
      socket.once('close', () => tunnels.delete(socket));
    }
    upstream.on('error', () => client.destroy());
    client.on('error', () => upstream.destroy());
  });
  await new Promise<void>((done) => server.listen(0, '127.0.0.1', done));
  const address = server.address();
  if (address === null || typeof address === 'string') {
    throw new Error('Serveur web sans port');
  }
  return { server, port: address.port };
}

async function waitForHealth(port: number, child: ChildProcess): Promise<void> {
  const deadline = Date.now() + 15_000;
  while (Date.now() < deadline) {
    if (child.exitCode !== null) {
      throw new Error(`Le serveur s'est arrêté au démarrage (code ${child.exitCode}).`);
    }
    try {
      const response = await fetch(`http://127.0.0.1:${port}/health`);
      if (response.ok) {
        return;
      }
    } catch {
      // pas encore à l'écoute : on réessaie
    }
    await new Promise((done) => setTimeout(done, 50));
  }
  throw new Error('Le serveur de jeu ne répond pas sur /health.');
}

/** Une pile complète pour un test : un serveur de jeu graine fixe + le client construit, sur des ports libres. */
export async function startStack(options: StackOptions): Promise<Stack> {
  if (!existsSync(SERVER_BINARY)) {
    throw new Error(
      `Serveur introuvable : ${SERVER_BINARY} (cmake --preset e2e && cmake --build --preset e2e, dans server/).`,
    );
  }
  if (!existsSync(join(CLIENT_DIST, 'index.html'))) {
    throw new Error(`Client non construit : ${CLIENT_DIST} (npm run build, dans client/).`);
  }
  const gamePort = await freePort();
  const tunnels = new Set<Socket>();
  const web = await startWebServer(() => gamePort, tunnels);
  const url = `http://127.0.0.1:${web.port}`;
  const child = spawn(SERVER_BINARY, [], {
    cwd: dirname(SERVER_BINARY),
    stdio: ['ignore', 'inherit', 'inherit'],
    env: {
      ...process.env,
      UNO_PORT: String(gamePort),
      UNO_ALLOWED_ORIGINS: url,
      UNO_TEST_SEED: String(options.seed),
      UNO_TEST_RECONNECT_GRACE_MS: String(options.reconnectGraceMs ?? 60_000),
      UNO_LOG_LEVEL: 'error',
    },
  });
  try {
    await waitForHealth(gamePort, child);
  } catch (error) {
    child.kill();
    web.server.close();
    throw error;
  }
  return {
    url,
    async stop() {
      const startedAt = Date.now();
      for (const socket of tunnels) {
        socket.destroy();
      }
      web.server.closeAllConnections();
      const webClosed = new Promise<void>((done) => web.server.close(() => done()));
      const exit = await stopServerProcess(child);
      await webClosed;
      const elapsedMs = Date.now() - startedAt;
      if (process.env['CI'] !== undefined || elapsedMs > 1000) {
        console.log(
          `[stack] démontage : pid ${child.pid}, ${exit}, ${elapsedMs} ms, port ${gamePort}`,
        );
      }
    },
  };
}

/** SIGTERM, puis SIGKILL si le serveur n'est pas sorti au bout d'une seconde ; décrit comment il s'est arrêté. */
async function stopServerProcess(child: ChildProcess): Promise<string> {
  const describe = () => `sortie code ${child.exitCode} signal ${child.signalCode}`;
  if (child.exitCode !== null || child.signalCode !== null) {
    return describe();
  }
  const exited = new Promise<void>((done) => child.once('exit', () => done()));
  child.kill();
  const timer = setTimeout(() => child.kill('SIGKILL'), 1000);
  await exited;
  clearTimeout(timer);
  const killed = child.signalCode === 'SIGKILL' ? ' (SIGKILL : le serveur ne s’arrêtait pas)' : '';
  return `${describe()}${killed}`;
}
