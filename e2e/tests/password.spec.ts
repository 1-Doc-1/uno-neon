import { connect } from 'node:net';
import { expect, test } from '../support/fixtures.ts';
import { chooseSetting, startMatch } from '../support/flow.ts';
import { playUntil } from '../support/player.ts';
import { SEEDS } from '../support/seeds.ts';

// L'accès protégé par mot de passe (ADR 0043) : le jeu est servi derrière le vrai Caddy de deploy/Caddyfile.tunnel, avec
// SITE_PASSWORD_HASH. Ces tests exigent Caddy (`caddy` dans le PATH, ou CADDY_BIN).

const PASSWORD = 'soiree-entre-amis';

test.use({ seed: SEEDS.fastGame, sitePassword: PASSWORD });

/** La première ligne de la réponse à une poignée de main WebSocket, avec ou sans identifiants. */
async function websocketHandshakeStatus(url: string, authorization?: string): Promise<string> {
  const { hostname, port } = new URL(url);
  return new Promise((resolve, reject) => {
    const socket = connect(Number(port), hostname, () => {
      const lines = [
        'GET /ws HTTP/1.1',
        `Host: ${hostname}:${port}`,
        `Origin: ${url}`,
        'Upgrade: websocket',
        'Connection: Upgrade',
        'Sec-WebSocket-Version: 13',
        'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
      ];
      if (authorization !== undefined) {
        lines.push(`Authorization: ${authorization}`);
      }
      socket.write(`${lines.join('\r\n')}\r\n\r\n`);
    });
    socket.setTimeout(5000, () => socket.destroy(new Error('Pas de réponse à la poignée de main')));
    socket.once('data', (chunk: Buffer) => {
      resolve(chunk.toString('latin1').split('\r\n')[0] ?? '');
      socket.destroy();
    });
    socket.once('error', reject);
  });
}

const basic = (user: string, password: string) =>
  `Basic ${Buffer.from(`${user}:${password}`).toString('base64')}`;

test('sans mot de passe, la page et le WebSocket répondent 401', async ({ stack, request }) => {
  for (const path of ['/', '/index.html', '/comment-jouer', '/health']) {
    const response = await request.get(`${stack.url}${path}`, { headers: {} });
    expect(response.status(), path).toBe(401);
    expect(response.headers()['www-authenticate']).toMatch(/^Basic /);
  }
  expect(await websocketHandshakeStatus(stack.url)).toContain('401');
});

test('un mauvais mot de passe est refusé partout', async ({ stack }) => {
  const wrong = basic(stack.siteUser, `${PASSWORD}-faux`);
  const page = await fetch(stack.url, { headers: { Authorization: wrong } });
  expect(page.status).toBe(401);
  expect(await websocketHandshakeStatus(stack.url, wrong)).toContain('401');
});

test('avec le mot de passe, la page et le WebSocket répondent', async ({ stack }) => {
  const good = basic(stack.siteUser, PASSWORD);
  const page = await fetch(stack.url, { headers: { Authorization: good } });
  expect(page.status).toBe(200);
  expect(await page.text()).toContain('<app-root');
  expect(await websocketHandshakeStatus(stack.url, good)).toContain('101');
});

test('une partie complète se joue normalement derrière le mot de passe', async ({ lobby }) => {
  const { host, guest } = lobby;
  await chooseSetting(host, 'Manche unique');
  await startMatch(lobby);
  await playUntil([guest, host], () =>
    host.page.getByRole('heading', { name: 'Bob remporte la partie !' }).isVisible(),
  );
  for (const player of [host, guest]) {
    await expect(
      player.page.getByRole('heading', { name: 'Bob remporte la partie !' }),
    ).toBeVisible();
  }
});
