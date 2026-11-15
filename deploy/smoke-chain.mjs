// Contrôle de la chaîne de confiance sur les VRAIS conteneurs (deploy/smoke-test.sh, ADR 0044).
// Le script joue le rôle de Traefik : il parle à Caddy en écrivant des X-Forwarded-For, forgés ou non. Les limites par
// adresse du serveur (20 connexions) servent de témoin : elles ne comptent la bonne adresse que si Caddy croit Traefik et
// si le serveur ne croit que Caddy.
//   node deploy/smoke-chain.mjs <adresse-de-caddy> <port> <origine>
import { connect } from 'node:net';

const [host, portText, origin] = process.argv.slice(2);
const port = Number(portText);
if (!host || !port || !origin) {
  console.error('usage : node deploy/smoke-chain.mjs <adresse> <port> <origine>');
  process.exit(2);
}

function websocket(forwardedFor) {
  return new Promise((resolve) => {
    const socket = connect(port, host, () => {
      socket.write(
        [
          'GET /ws HTTP/1.1',
          `Host: ${new URL(origin).host}`,
          `Origin: ${origin}`,
          'Upgrade: websocket',
          'Connection: Upgrade',
          'Sec-WebSocket-Version: 13',
          'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==',
          `X-Forwarded-For: ${forwardedFor}`,
          '',
          '',
        ].join('\r\n'),
      );
    });
    socket.setTimeout(5000, () => {
      socket.destroy();
      resolve({ status: 'timeout', socket });
    });
    socket.once('data', (chunk) =>
      resolve({ status: chunk.toString('latin1').split('\r\n')[0], socket }),
    );
    socket.once('error', () => resolve({ status: 'error', socket }));
  });
}

async function openMany(count, forwardedFor) {
  const sockets = [];
  let accepted = 0;
  let refused = 0;
  for (let index = 1; index <= count; index += 1) {
    const reply = await websocket(forwardedFor(index));
    sockets.push(reply.socket);
    accepted += reply.status.includes('101') ? 1 : 0;
    refused += reply.status.includes('503') ? 1 : 0;
  }
  sockets.forEach((socket) => socket.destroy());
  return { accepted, refused };
}

let failed = false;
const expect = (label, actual, wanted) => {
  const ok = JSON.stringify(actual) === JSON.stringify(wanted);
  failed ||= !ok;
  console.log(`${ok ? 'OK  ' : 'ÉCHEC'} ${label} : ${JSON.stringify(actual)}${ok ? '' : ` (attendu ${JSON.stringify(wanted)})`}`);
};

// 25 visiteurs différents (adresse la plus à droite), chacun écrivant « 9.9.9.9 » à gauche : tous acceptés
expect(
  'visiteurs distincts, adresse forgée à gauche ignorée',
  await openMany(25, (index) => `9.9.9.9, 203.0.113.${index}`),
  { accepted: 25, refused: 0 },
);
await new Promise((done) => setTimeout(done, 500));
// Un seul visiteur : la limite de 20 connexions par adresse le rattrape
expect('un seul visiteur', await openMany(25, () => '198.51.100.7'), { accepted: 20, refused: 5 });

process.exit(failed ? 1 : 0);
