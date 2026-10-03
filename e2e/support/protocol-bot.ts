import { WebSocket } from 'ws';
import type {
  ClientEvent,
  Color,
  PlayerView,
} from '../../client/src/app/protocol/generated/protocol.ts';

interface Reply {
  readonly ok: boolean;
  readonly code?: string;
}

/**
 * Un joueur qui parle directement le protocole (sans navigateur). Il sert à chercher des graines : on y rejoue exactement
 * la politique de jeu que les tests E2E appliquent par l'interface (voir `choosePlay`).
 */
export class Bot {
  view: PlayerView | null = null;
  events: ClientEvent[] = [];
  playerId = '';
  roomCode = '';

  private socket!: WebSocket;
  private nextId = 1;
  private readonly pending = new Map<string, (reply: Reply) => void>();
  private readonly waiters: (() => void)[] = [];

  private readonly url: string;
  private readonly origin: string;

  constructor(url: string, origin: string) {
    this.url = url;
    this.origin = origin;
  }

  async open(): Promise<void> {
    this.socket = new WebSocket(this.url, { headers: { origin: this.origin } });
    this.socket.on('message', (data) =>
      this.receive(JSON.parse(String(data)) as Record<string, unknown>),
    );
    await new Promise<void>((done, fail) => {
      this.socket.once('open', done);
      this.socket.once('error', fail);
    });
    const welcome = new Promise<void>((done) => this.waiters.push(done));
    // Le serveur répond à session.hello par session.welcome (pas par un ack)
    void this.send('session.hello', { clientVersion: '0.1.0' });
    await welcome;
  }

  send(type: string, payload: object): Promise<Reply> {
    const id = `b-${this.nextId++}`;
    return new Promise((done) => {
      this.pending.set(id, done);
      this.socket.send(JSON.stringify({ v: 1, id, type, payload }));
    });
  }

  close(): void {
    this.socket.close();
  }

  /** Attend la prochaine mise à jour de partie. */
  nextUpdate(): Promise<void> {
    return new Promise((done) => this.waiters.push(done));
  }

  private receive(message: Record<string, unknown>): void {
    const payload = message['payload'] as Record<string, unknown> | undefined;
    switch (message['type']) {
      case 'ack':
      case 'error': {
        const resolve = this.pending.get(String(message['replyTo']));
        this.pending.delete(String(message['replyTo']));
        resolve?.({ ok: message['type'] === 'ack', code: payload?.['code'] as string | undefined });
        break;
      }
      case 'session.welcome':
        this.playerId = String(payload?.['playerId']);
        break;
      case 'room.update':
        this.roomCode = String(
          (payload?.['room'] as Record<string, unknown> | undefined)?.['code'] ?? this.roomCode,
        );
        break;
      case 'game.update':
        this.view = payload?.['view'] as PlayerView;
        this.events.push(...((payload?.['events'] as ClientEvent[] | undefined) ?? []));
        break;
      default:
        break;
    }
    if (message['type'] === 'session.welcome' || message['type'] === 'game.update') {
      this.waiters.splice(0).forEach((wake) => wake());
    }
  }
}

export type Move =
  | { readonly type: 'game.respondPenalty'; readonly payload: { response: 'accept' | 'challenge' } }
  | { readonly type: 'game.chooseColor'; readonly payload: { color: Color } }
  | { readonly type: 'game.playCard'; readonly payload: { cardId: number; chosenColor?: Color } };

/** La couleur que la politique choisit toujours : la première tuile du sélecteur. */
export const POLICY_COLOR: Color = 'red';

/**
 * La politique de jeu des tests : accepter une pénalité, choisir rouge, sinon jouer la première carte jouable de la
 * main (dans l'ordre d'affichage). Quand rien ne se joue, le serveur pioche de lui-même (pioche guidée) : `null`.
 */
export function choosePlay(view: PlayerView): Move | null {
  const { me } = view;
  if (me.penaltyResponse) {
    return { type: 'game.respondPenalty', payload: { response: 'accept' } };
  }
  if (me.canChooseColor) {
    return { type: 'game.chooseColor', payload: { color: POLICY_COLOR } };
  }
  if (view.currentPlayerId !== me.playerId) {
    return null;
  }
  const card = me.hand.find((candidate) => me.playableCardIds.includes(candidate.id));
  if (!card) {
    return null;
  }
  return {
    type: 'game.playCard',
    payload:
      card.color === null ? { cardId: card.id, chosenColor: POLICY_COLOR } : { cardId: card.id },
  };
}
