import { signal } from '@angular/core';
import { Observable, Subject } from 'rxjs';
import type { ClientMessage, ServerMessage } from '../protocol/generated/protocol';
import type { GameTransport, TransportStatus } from './game-transport';
import { parseServerMessage } from './parse-server-message';

const FIRST_DELAY_MS = 500;
const MAX_DELAY_MS = 8000;
/** Fermeture envoyée par le serveur quand une connexion plus récente reprend la session. */
const SESSION_TAKEN_OVER = 4000;

/** Vraie connexion WebSocket, avec reconnexion à backoff exponentiel plafonné et jitter (SPEC §10.3). */
export class WebSocketTransport implements GameTransport {
  private readonly statusState = signal<TransportStatus>('closed');
  private readonly attemptState = signal(0);
  private readonly incoming = new Subject<ServerMessage>();
  private socket: WebSocket | null = null;
  private retryTimer: ReturnType<typeof setTimeout> | null = null;
  private wanted = false;

  readonly status = this.statusState.asReadonly();
  readonly attempt = this.attemptState.asReadonly();
  readonly messages: Observable<ServerMessage> = this.incoming.asObservable();

  constructor(
    private readonly url: string,
    private readonly createSocket: (url: string) => WebSocket = (u) => new WebSocket(u),
  ) {}

  connect(): void {
    if (this.wanted) {
      return;
    }
    this.wanted = true;
    this.open();
  }

  disconnect(): void {
    this.wanted = false;
    this.clearRetry();
    this.socket?.close();
    this.socket = null;
    this.attemptState.set(0);
    this.statusState.set('closed');
  }

  send(message: ClientMessage): boolean {
    if (this.socket?.readyState !== WebSocket.OPEN) {
      return false;
    }
    this.socket.send(JSON.stringify(message));
    return true;
  }

  private open(): void {
    this.statusState.set(this.attemptState() === 0 ? 'connecting' : 'reconnecting');
    const socket = this.createSocket(this.url);
    this.socket = socket;
    socket.onopen = () => {
      this.attemptState.set(0);
      this.statusState.set('open');
    };
    socket.onmessage = (event: MessageEvent<unknown>) => {
      const message = typeof event.data === 'string' ? parseServerMessage(event.data) : null;
      if (message) {
        this.incoming.next(message);
      }
    };
    socket.onclose = (event) => {
      if (this.socket !== socket) {
        return;
      }
      this.socket = null;
      if (this.wanted && event.code !== SESSION_TAKEN_OVER) {
        this.scheduleReconnect();
      } else {
        this.wanted = false;
        this.statusState.set('closed');
      }
    };
  }

  private scheduleReconnect(): void {
    this.attemptState.update((n) => n + 1);
    this.statusState.set('reconnecting');
    const ceiling = Math.min(MAX_DELAY_MS, FIRST_DELAY_MS * 2 ** (this.attemptState() - 1));
    const delay = ceiling * (0.5 + Math.random() / 2);
    this.retryTimer = setTimeout(() => this.open(), delay);
  }

  private clearRetry(): void {
    if (this.retryTimer !== null) {
      clearTimeout(this.retryTimer);
      this.retryTimer = null;
    }
  }
}
