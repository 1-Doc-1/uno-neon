import type { ServerMessage } from '../protocol/generated/protocol';

const SERVER_MESSAGE_TYPES: ReadonlySet<string> = new Set([
  'ack',
  'error',
  'session.welcome',
  'room.update',
  'game.update',
  'reaction',
  'room.closed',
] satisfies ServerMessage['type'][]);

/**
 * Décode une trame texte du serveur. Seule l'enveloppe est vérifiée : la forme des charges utiles
 * est garantie par le contrat du protocole (schémas + tests de contrat), le serveur étant de confiance.
 */
export function parseServerMessage(text: string): ServerMessage | null {
  let data: unknown;
  try {
    data = JSON.parse(text);
  } catch {
    return null;
  }
  if (typeof data !== 'object' || data === null) {
    return null;
  }
  const { v, type } = data as { v?: unknown; type?: unknown };
  if (v !== 1 || typeof type !== 'string' || !SERVER_MESSAGE_TYPES.has(type)) {
    return null;
  }
  return data as ServerMessage;
}
