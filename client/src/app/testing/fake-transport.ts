import { signal } from '@angular/core';
import { Subject } from 'rxjs';
import type { GameTransport, TransportStatus } from '../core/game-transport';
import type { ClientMessage, ServerMessage } from '../protocol/generated/protocol';

/** Transport de test : enregistre les messages envoyés et laisse le test jouer le rôle du serveur. */
export class FakeTransport implements GameTransport {
  readonly statusState = signal<TransportStatus>('closed');
  readonly status = this.statusState.asReadonly();
  readonly attempt = signal(0);
  readonly incoming = new Subject<ServerMessage>();
  readonly messages = this.incoming.asObservable();
  readonly sent: ClientMessage[] = [];

  connect(): void {
    this.statusState.set('open');
  }
  disconnect(): void {
    this.statusState.set('closed');
  }
  send(message: ClientMessage): boolean {
    this.sent.push(message);
    return true;
  }
  receive(message: ServerMessage): void {
    this.incoming.next(message);
  }
}
