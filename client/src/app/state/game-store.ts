import { computed, effect, inject, Service, signal, untracked } from '@angular/core';
import { describeError } from '../core/error-messages';
import { GAME_TRANSPORT } from '../core/game-transport';
import { SessionService } from '../core/session.service';
import type {
  ClientEvent,
  ClientMessage,
  Color,
  ErrorPayload,
  PlayerView,
  RoomClosedPayload,
  RoomSettingsPatch,
  RoomView,
  ServerMessage,
} from '../protocol/generated/protocol';

type RequestType = ClientMessage['type'];
type PayloadOf<T extends RequestType> = Extract<ClientMessage, { type: T }>['payload'];

/** Les événements d'une mise à jour de partie, pour les animations. `resync` : vue complète, il n'y a rien à rejouer. */
export interface EventBatch {
  readonly id: number;
  readonly events: readonly ClientEvent[];
  readonly resync: boolean;
}

export interface Notice {
  readonly id: number;
  readonly text: string;
}

interface PendingRequest {
  readonly settle: (error: ErrorPayload | null) => void;
}

const NOTICE_LIFETIME_MS = 5000;
const LOG_SIZE = 20;
const CLIENT_VERSION = '0.1.0';

/**
 * Façade entre les écrans et le serveur : expose l'état en signals en lecture seule et les intentions du
 * joueur. Le serveur reste l'unique autorité : ici on ne fait que relayer des demandes et afficher la vérité
 * reçue (SPEC §10.4).
 */
@Service()
export class GameStore {
  private readonly transport = inject(GAME_TRANSPORT);
  private readonly session = inject(SessionService);

  private readonly pending = new Map<string, PendingRequest>();
  private nextRequestId = 1;
  private nextNoticeId = 1;
  private roomVersion = -1;
  private nextBatchId = 1;
  /** Après une coupure, la prochaine mise à jour est une vue complète : rien à rejouer. */
  private resyncNext = false;

  private readonly playerIdState = signal<string | null>(null);
  private readonly readyState = signal(false);
  private readonly roomState = signal<RoomView | null>(null);
  private readonly viewState = signal<PlayerView | null>(null);
  private readonly serverClockOffsetState = signal(0);
  private readonly resumedRoomState = signal<string | null>(null);
  private readonly closedState = signal<RoomClosedPayload['reason'] | null>(null);
  private readonly recentEventsState = signal<readonly ClientEvent[]>([]);
  private readonly eventBatchState = signal<EventBatch | null>(null);
  private readonly noticesState = signal<readonly Notice[]>([]);

  readonly connection = this.transport.status;
  readonly reconnectAttempt = this.transport.attempt;
  /** Vrai une fois la session établie (`session.welcome` reçu) sur la connexion courante. */
  readonly ready = this.readyState.asReadonly();
  readonly playerId = this.playerIdState.asReadonly();
  readonly room = this.roomState.asReadonly();
  readonly view = this.viewState.asReadonly();
  /** Pourquoi le salon a fermé sous nos pieds (exclusion, expiration), le cas échéant. */
  readonly closedReason = this.closedState.asReadonly();
  /** Les derniers événements de la partie, du plus ancien au plus récent (journal de la table). */
  readonly recentEvents = this.recentEventsState.asReadonly();
  /** Le dernier lot d'événements reçu : les animations le consomment (elles ne comparent jamais deux vues). */
  readonly eventBatch = this.eventBatchState.asReadonly();
  readonly notices = this.noticesState.asReadonly();

  /** Vrai tant que la session n'est pas établie ou que le salon repris n'est pas encore arrivé. */
  readonly loading = computed(
    () => !this.readyState() || (this.resumedRoomState() !== null && this.roomState() === null),
  );
  readonly roomCode = computed(() => this.roomState()?.code ?? null);
  readonly me = computed(() =>
    this.roomState()?.players.find((p) => p.playerId === this.playerId()),
  );
  readonly isHost = computed(() => this.me()?.isHost ?? false);
  readonly isMyTurn = computed(() => {
    const view = this.viewState();
    return view !== null && view.currentPlayerId === view.me.playerId;
  });
  /** Écart entre l'horloge du serveur et celle du navigateur, pour afficher les comptes à rebours. */
  readonly serverClockOffset = this.serverClockOffsetState.asReadonly();

  constructor() {
    this.transport.messages.subscribe((message) => this.receive(message));
    effect(() => {
      if (this.transport.status() === 'open') {
        untracked(() => void this.establishSession());
      } else {
        untracked(() => this.dropConnection());
      }
    });
    this.transport.connect();
  }

  // ---- Salon ----

  createRoom(nickname: string): Promise<boolean> {
    this.session.saveNickname(nickname);
    return this.request('room.create', { nickname });
  }

  joinRoom(code: string, nickname: string): Promise<boolean> {
    this.session.saveNickname(nickname);
    return this.request('room.join', { code, nickname });
  }

  async leaveRoom(): Promise<boolean> {
    const ok = await this.request('room.leave', {});
    if (ok) {
      this.forgetRoom();
    }
    return ok;
  }

  setReady(ready: boolean): Promise<boolean> {
    return this.request('room.setReady', { ready });
  }

  updateSettings(settings: RoomSettingsPatch): Promise<boolean> {
    return this.request('room.updateSettings', { settings });
  }

  kick(playerId: string): Promise<boolean> {
    return this.request('room.kick', { playerId });
  }

  startMatch(): Promise<boolean> {
    return this.request('match.start', {});
  }

  readyForNextRound(): Promise<boolean> {
    return this.request('match.readyForNextRound', {});
  }

  /** Efface le message « salon fermé » une fois lu. */
  acknowledgeClosed(): void {
    this.closedState.set(null);
  }

  // ---- Partie ----

  /** Poser une carte : un Joker porte sa couleur, un Joker +5 aussi la cible qu'il vise. */
  playCard(cardId: number, chosenColor?: Color, targetId?: string): Promise<boolean> {
    return this.request('game.playCard', {
      cardId,
      ...(chosenColor ? { chosenColor } : {}),
      ...(targetId ? { targetId } : {}),
    });
  }

  chooseColor(color: Color): Promise<boolean> {
    return this.request('game.chooseColor', { color });
  }

  draw(): Promise<boolean> {
    return this.request('game.drawCard', {});
  }

  pass(): Promise<boolean> {
    return this.request('game.pass', {});
  }

  callUno(): Promise<boolean> {
    return this.request('game.callUno', {});
  }

  catchUno(targetId: string): Promise<boolean> {
    return this.request('game.catchUno', { targetId });
  }

  respondPenalty(response: 'accept' | 'challenge'): Promise<boolean> {
    return this.request('game.respondPenalty', { response });
  }

  // ---- Interne ----

  private async establishSession(): Promise<void> {
    const sessionToken = this.session.token();
    const error = await this.exchange('session.hello', {
      clientVersion: CLIENT_VERSION,
      ...(sessionToken ? { sessionToken } : {}),
    });
    if (error?.code === 'SESSION_EXPIRED') {
      this.session.clearToken();
      this.forgetRoom();
      this.notify(describeError('SESSION_EXPIRED'));
      await this.establishSession();
    } else if (error) {
      this.notify(describeError(error.code, error.details?.reason));
    }
  }

  private dropConnection(): void {
    this.readyState.set(false);
    this.resyncNext = true;
    for (const request of this.pending.values()) {
      request.settle({ code: 'SESSION_REQUIRED', message: 'connection lost' });
    }
    this.pending.clear();
  }

  /** Envoie une demande de joueur ; résout `true` si le serveur l'accepte, sinon affiche l'erreur en français. */
  private async request<T extends RequestType>(type: T, payload: PayloadOf<T>): Promise<boolean> {
    if (!this.readyState()) {
      this.notify(describeError('SESSION_REQUIRED'));
      return false;
    }
    const error = await this.exchange(type, payload);
    if (error) {
      this.notify(describeError(error.code, error.details?.reason));
    }
    return error === null;
  }

  private exchange<T extends RequestType>(
    type: T,
    payload: PayloadOf<T>,
  ): Promise<ErrorPayload | null> {
    const id = `c-${this.nextRequestId++}`;
    // La correspondance type ↔ charge utile est garantie par PayloadOf ; TypeScript ne sait pas la
    // recombiner dans l'union discriminée.
    const message = { v: 1, id, type, payload } as ClientMessage;
    return new Promise((resolve) => {
      this.pending.set(id, { settle: resolve });
      if (!this.transport.send(message)) {
        this.pending.delete(id);
        resolve({ code: 'SESSION_REQUIRED', message: 'not connected' });
      }
    });
  }

  private receive(message: ServerMessage): void {
    switch (message.type) {
      case 'ack':
        this.settle(message.replyTo, null);
        break;
      case 'error':
        if (message.replyTo !== undefined && this.pending.has(message.replyTo)) {
          this.settle(message.replyTo, message.payload);
        } else {
          this.notify(describeError(message.payload.code, message.payload.details?.reason));
        }
        break;
      case 'session.welcome':
        this.onWelcome(
          message.payload.sessionToken,
          message.payload.playerId,
          message.payload.resumedRoomCode,
        );
        break;
      case 'room.update':
        this.onRoomUpdate(message.payload.roomVersion, message.payload.room);
        break;
      case 'game.update':
        this.onGameUpdate(message.payload.serverTime, message.payload.events, message.payload.view);
        break;
      case 'room.closed':
        this.forgetRoom();
        this.closedState.set(message.payload.reason);
        break;
      case 'reaction':
        break;
    }
  }

  private settle(id: string, error: ErrorPayload | null): void {
    const request = this.pending.get(id);
    if (request) {
      this.pending.delete(id);
      request.settle(error);
    }
  }

  private onWelcome(token: string, playerId: string, resumedRoomCode: string | undefined): void {
    this.session.saveToken(token);
    this.playerIdState.set(playerId);
    this.resumedRoomState.set(resumedRoomCode ?? null);
    if (resumedRoomCode === undefined) {
      this.forgetRoom();
    }
    this.readyState.set(true);
  }

  private onRoomUpdate(version: number, room: RoomView): void {
    if (room.code === this.roomState()?.code && version <= this.roomVersion) {
      return;
    }
    this.roomVersion = version;
    this.resumedRoomState.set(null);
    this.closedState.set(null);
    this.roomState.set(room);
  }

  /** La vue complète fait foi ; une vue plus ancienne que celle déjà affichée (message tardif) est ignorée. */
  private onGameUpdate(serverTime: number, events: ClientEvent[], view: PlayerView): void {
    const current = this.viewState();
    if (current && view.stateVersion < current.stateVersion) {
      return;
    }
    this.serverClockOffsetState.set(serverTime - Date.now());
    if (!current || view.stateVersion > current.stateVersion) {
      this.recentEventsState.update((log) => [...log, ...events].slice(-LOG_SIZE));
      const missedUpdates = current !== null && view.stateVersion !== current.stateVersion + 1;
      this.eventBatchState.set({
        id: this.nextBatchId++,
        events,
        resync: current === null || missedUpdates || this.resyncNext,
      });
      this.resyncNext = false;
    }
    this.viewState.set(view);
  }

  private forgetRoom(): void {
    this.roomVersion = -1;
    this.resumedRoomState.set(null);
    this.roomState.set(null);
    this.viewState.set(null);
    this.recentEventsState.set([]);
    this.eventBatchState.set(null);
  }

  notify(text: string): void {
    const notice: Notice = { id: this.nextNoticeId++, text };
    this.noticesState.update((list) => [...list, notice]);
    setTimeout(() => this.dismissNotice(notice.id), NOTICE_LIFETIME_MS);
  }

  dismissNotice(id: number): void {
    this.noticesState.update((list) => list.filter((n) => n.id !== id));
  }
}
