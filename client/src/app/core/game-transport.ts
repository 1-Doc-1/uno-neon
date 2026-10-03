import { InjectionToken, Signal } from '@angular/core';
import { Observable } from 'rxjs';
import type { ClientMessage, ServerMessage } from '../protocol/generated/protocol';

export type TransportStatus = 'connecting' | 'open' | 'reconnecting' | 'closed';

/**
 * Canal de messages typés entre le client et le serveur (Adapter, SPEC §10.3).
 * Les stores ne savent pas s'il s'agit d'un vrai WebSocket ou d'un scénario écrit à la main.
 */
export interface GameTransport {
  readonly status: Signal<TransportStatus>;
  /** Numéro de la tentative de reconnexion en cours (0 quand la connexion est ouverte). */
  readonly attempt: Signal<number>;
  readonly messages: Observable<ServerMessage>;
  connect(): void;
  disconnect(): void;
  /** Renvoie `false` si le message n'a pas pu partir (connexion fermée). */
  send(message: ClientMessage): boolean;
}

export const GAME_TRANSPORT = new InjectionToken<GameTransport>('GameTransport');
