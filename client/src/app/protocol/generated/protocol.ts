/*
 * GÉNÉRÉ AUTOMATIQUEMENT depuis protocol/schema par `npm run protocol:gen`.
 * Ne pas modifier à la main : modifier le schéma JSON puis relancer la génération.
 */

/**
 * Entry point of protocol v1: a WebSocket text frame is either a client message or a server message.
 */
export type Protocol = ClientMessage | ServerMessage;
/**
 * Every message a client may send (intentions only: the server validates everything). The server checks `v` then `type` before the payload, to answer UNSUPPORTED_VERSION or UNKNOWN_TYPE; any other violation is MALFORMED_MESSAGE.
 */
export type ClientMessage =
  | HelloMessage
  | CreateRoomMessage
  | JoinRoomMessage
  | LeaveRoomMessage
  | UpdateSettingsMessage
  | SetReadyMessage
  | KickMessage
  | AddBotMessage
  | StartMatchMessage
  | RematchMessage
  | ReadyForNextRoundMessage
  | PlayCardMessage
  | DrawCardMessage
  | PassMessage
  | ChooseColorMessage
  | RespondPenaltyMessage
  | CallUnoMessage
  | CatchUnoMessage
  | SendReactionMessage;
/**
 * Value of the `v` field of every envelope.
 */
export type ProtocolVersion = 1;
/**
 * Client-chosen identifier, unique per connection, echoed in `replyTo`.
 */
export type MessageId = string;
/**
 * 128 cryptographically random bits, base64url without padding.
 */
export type SessionToken = string;
/**
 * Raw nickname. The server trims it and checks the rules (2 to 16 letters, digits, space, _ or -), answering NICKNAME_INVALID: only the size is bounded here.
 */
export type Nickname = string;
/**
 * official: SPEC §3. ladder: penalty cards pile up, +2 < +4 < +5, and a +4 cannot be challenged (ADR 0029).
 */
export type PenaltyStacking = 'official' | 'ladder';
/**
 * untilPlayable: a player with nothing to play draws until a card fits (ADR 0024); one: a single card (SPEC §3).
 */
export type DrawAmount = 'untilPlayable' | 'one';
export type WildDrawFourMode = 'officialChallenge' | 'strict';
/**
 * 0 disables the turn timer.
 */
export type TurnTimerSeconds = 0 | 15 | 30 | 60;
/**
 * singleRound: one round decides the winner; to250 / to500: first player to reach the score.
 */
export type MatchLength = 'singleRound' | 'to250' | 'to500';
export type MaxPlayers = number;
/**
 * guided: no pointless draws, a plain drawn card is played automatically (ADR 0017); official: SPEC §3.
 */
export type DrawRule = 'guided' | 'official';
/**
 * Draw Two in the deck: 8 x this (ADR 0028).
 */
export type CardMultiplier = 1 | 2 | 3 | 5;
/**
 * Wild Draw Four in the deck: 4 x this (ADR 0028).
 */
export type CardMultiplier1 = 1 | 2 | 3 | 5;
/**
 * Wild Draw Five in the deck: 2 x this (ADR 0028).
 */
export type CardMultiplier2 = 1 | 2 | 3 | 5;
/**
 * 6 characters, without the ambiguous I, L, O, 0 and 1.
 */
export type RoomCode = string;
/**
 * Payload of messages without parameters. tsType is a json-schema-to-typescript extension: an empty object type that rejects any property.
 */
export type EmptyPayload = Record<string, never>;
/**
 * Opaque random identifier of a player (never a seat number).
 */
export type PlayerId = string;
export type BotStrategy = 'random' | 'greedy';
/**
 * Identifier of a card, randomly assigned for each match: it reveals nothing about the card.
 */
export type CardId = number;
export type Color = 'red' | 'yellow' | 'green' | 'blue';
export type Emote = 'gg' | 'wow' | 'lol' | 'ouch' | 'think' | 'fire';
/**
 * Every message the server may send: replies (ack, error) and pushed messages.
 */
export type ServerMessage =
  | AckMessage
  | ErrorMessage
  | WelcomeMessage
  | RoomUpdateMessage
  | GameUpdateMessage
  | ReactionMessage
  | RoomClosedMessage;
/**
 * Every error the server can answer. MESSAGE_TOO_LARGE is reported as the WebSocket close reason (code 1009), not as an `error` message.
 */
export type ErrorCode =
  | 'MALFORMED_MESSAGE'
  | 'UNKNOWN_TYPE'
  | 'UNSUPPORTED_VERSION'
  | 'MESSAGE_TOO_LARGE'
  | 'RATE_LIMITED'
  | 'SESSION_REQUIRED'
  | 'SESSION_EXPIRED'
  | 'NICKNAME_INVALID'
  | 'NICKNAME_TAKEN'
  | 'ALREADY_IN_ROOM'
  | 'NOT_IN_ROOM'
  | 'ROOM_NOT_FOUND'
  | 'ROOM_FULL'
  | 'MATCH_IN_PROGRESS'
  | 'NOT_HOST'
  | 'CANNOT_KICK_SELF'
  | 'INVALID_SETTINGS'
  | 'NOT_ENOUGH_PLAYERS'
  | 'PLAYERS_NOT_READY'
  | 'NOT_YOUR_TURN'
  | 'INVALID_PHASE'
  | 'CARD_NOT_IN_HAND'
  | 'ILLEGAL_MOVE'
  | 'UNO_WINDOW_CLOSED'
  | 'UNO_GRACE_PERIOD'
  | 'EFFECT_IN_PROGRESS';
/**
 * Detail of an ILLEGAL_MOVE error.
 */
export type IllegalMoveReason =
  | 'COLOR_MISMATCH'
  | 'WILD_DRAW_FOUR_ILLEGAL'
  | 'COLOR_REQUIRED'
  | 'COLOR_NOT_ALLOWED'
  | 'SWAP_TARGET_REQUIRED'
  | 'SWAP_TARGET_INVALID'
  | 'ONLY_DRAWN_CARD_PLAYABLE'
  | 'JUMP_IN_TOO_LATE'
  | 'CANNOT_STACK'
  | 'CANNOT_CHALLENGE'
  | 'MUST_PLAY'
  | 'MUST_DECLARE_UNO'
  | 'TARGET_REQUIRED'
  | 'TARGET_NOT_ALLOWED'
  | 'INVALID_TARGET'
  | 'ONLY_PLUS_FIVE_PLAYABLE';
/**
 * Draw Two in the deck: 8 x this (ADR 0028).
 */
export type CardMultiplier3 = 1 | 2 | 3 | 5;
/**
 * Wild Draw Four in the deck: 4 x this (ADR 0028).
 */
export type CardMultiplier4 = 1 | 2 | 3 | 5;
/**
 * Wild Draw Five in the deck: 2 x this (ADR 0028).
 */
export type CardMultiplier5 = 1 | 2 | 3 | 5;
/**
 * Server clock, milliseconds since the Unix epoch.
 */
export type EpochMillis = number;
/**
 * Projected domain event, used by the client to animate before applying the view. Discriminated by `kind`.
 */
export type ClientEvent =
  | RoundStartedEvent
  | CardPlayedEvent
  | CardsDrawnEvent
  | TurnChangedEvent
  | PlayerSkippedEvent
  | DirectionChangedEvent
  | ColorChosenEvent
  | PenaltyStackedEvent
  | ChallengeResolvedEvent
  | PlusFiveTargetedEvent
  | UnoCalledEvent
  | UnoCaughtEvent
  | HandsSwappedEvent
  | HandsRotatedEvent
  | DeckReshuffledEvent
  | RoundEndedEvent
  | MatchEndedEvent
  | PlayerDisconnectedEvent
  | PlayerReconnectedEvent
  | HostChangedEvent;
export type Rank =
  | '0'
  | '1'
  | '2'
  | '3'
  | '4'
  | '5'
  | '6'
  | '7'
  | '8'
  | '9'
  | 'skip'
  | 'reverse'
  | 'drawTwo'
  | 'wild'
  | 'wildDrawFour'
  | 'wildDrawFive';
export type Direction = 'clockwise' | 'counterClockwise';
export type GamePhase =
  | 'awaitingPlay'
  | 'awaitingDrawnCardDecision'
  | 'awaitingPenaltyResponse'
  | 'awaitingColorChoice'
  | 'roundOver'
  | 'matchOver';
/**
 * Server clock, milliseconds since the Unix epoch.
 */
export type EpochMillis1 = number;

/**
 * First message of every connection. Without a token a new session is created; with a valid token the session (and its room) is resumed.
 */
export interface HelloMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'session.hello';
  payload: HelloPayload;
}
export interface HelloPayload {
  sessionToken?: SessionToken;
  clientVersion: string;
}
/**
 * Creates a room and makes the sender its host. Not allowed while already in a room.
 */
export interface CreateRoomMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.create';
  payload: CreateRoomPayload;
}
export interface CreateRoomPayload {
  nickname: Nickname;
  settings?: RoomSettingsPatch;
}
/**
 * Partial settings: only the fields to change.
 */
export interface RoomSettingsPatch {
  stacking?: PenaltyStacking;
  jumpIn?: boolean;
  sevenZero?: boolean;
  drawAmount?: DrawAmount;
  wildDrawFourMode?: WildDrawFourMode;
  turnTimerSeconds?: TurnTimerSeconds;
  matchLength?: MatchLength;
  maxPlayers?: MaxPlayers;
  drawRule?: DrawRule;
  /**
   * House rule (ADR 0019): the last card cannot be played before UNO is announced.
   */
  declareUnoToWin?: boolean;
  drawTwoMultiplier?: CardMultiplier;
  wildDrawFourMultiplier?: CardMultiplier1;
  wildDrawFiveMultiplier?: CardMultiplier2;
}
/**
 * Joins an existing room that is in the lobby and not full.
 */
export interface JoinRoomMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.join';
  payload: JoinRoomPayload;
}
export interface JoinRoomPayload {
  code: RoomCode;
  nickname: Nickname;
}
/**
 * Leaves the current room.
 */
export interface LeaveRoomMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.leave';
  payload: EmptyPayload;
}
/**
 * Host only, in the lobby. INVALID_SETTINGS if the result is inconsistent (e.g. maxPlayers below the current number of players).
 */
export interface UpdateSettingsMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.updateSettings';
  payload: UpdateSettingsPayload;
}
export interface UpdateSettingsPayload {
  settings: RoomSettingsPatch;
}
/**
 * Marks the sender as ready (or not) in the lobby.
 */
export interface SetReadyMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.setReady';
  payload: SetReadyPayload;
}
export interface SetReadyPayload {
  ready: boolean;
}
/**
 * Host only, in the lobby: removes a player.
 */
export interface KickMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.kick';
  payload: KickPayload;
}
export interface KickPayload {
  playerId: PlayerId;
}
/**
 * Host only, in the lobby: adds a bot (phase 5).
 */
export interface AddBotMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'room.addBot';
  payload: AddBotPayload;
}
export interface AddBotPayload {
  strategy: BotStrategy;
}
/**
 * Host only: at least 2 players, all ready.
 */
export interface StartMatchMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'match.start';
  payload: EmptyPayload;
}
/**
 * Host only, once the match is over: same room, scores reset.
 */
export interface RematchMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'match.rematch';
  payload: EmptyPayload;
}
/**
 * After a round ends: the next round starts as soon as every connected player sent it, or at nextRoundDeadline.
 */
export interface ReadyForNextRoundMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'match.readyForNextRound';
  payload: EmptyPayload;
}
/**
 * Plays a card on your turn (or out of turn with jumpIn). chosenColor is required for wild cards, swapTargetId for a 7 with sevenZero, targetId for a Wild Draw Five (any other player; also the way to answer a Wild Draw Five aimed at you).
 */
export interface PlayCardMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.playCard';
  payload: PlayCardPayload;
}
export interface PlayCardPayload {
  cardId: CardId;
  chosenColor?: Color;
  swapTargetId?: PlayerId;
  targetId?: PlayerId;
}
/**
 * Draws instead of playing (phase awaitingPlay).
 */
export interface DrawCardMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.drawCard';
  payload: EmptyPayload;
}
/**
 * Ends the turn after drawing a card (phase awaitingDrawnCardDecision).
 */
export interface PassMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.pass';
  payload: EmptyPayload;
}
/**
 * Only when the first card turned over is a Wild: the first player picks the color (phase awaitingColorChoice).
 */
export interface ChooseColorMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.chooseColor';
  payload: ChooseColorPayload;
}
export interface ChooseColorPayload {
  color: Color;
}
/**
 * Answer of the targeted player to a pending draw penalty. Stacking is done with game.playCard.
 */
export interface RespondPenaltyMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.respondPenalty';
  payload: RespondPenaltyPayload;
}
export interface RespondPenaltyPayload {
  response: 'accept' | 'challenge';
}
/**
 * Announces UNO: with 2 cards on your turn, or with 1 card while the UNO window is open.
 */
export interface CallUnoMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.callUno';
  payload: EmptyPayload;
}
/**
 * Catches a player who did not announce UNO while the window is open.
 */
export interface CatchUnoMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'game.catchUno';
  payload: CatchUnoPayload;
}
export interface CatchUnoPayload {
  targetId: PlayerId;
}
/**
 * Predefined quick reaction (no free chat). At most one every 2 seconds.
 */
export interface SendReactionMessage {
  v: ProtocolVersion;
  id: MessageId;
  type: 'reaction.send';
  payload: SendReactionPayload;
}
export interface SendReactionPayload {
  emote: Emote;
}
/**
 * The client message `replyTo` was accepted.
 */
export interface AckMessage {
  v: ProtocolVersion;
  type: 'ack';
  replyTo: MessageId;
}
/**
 * The client message `replyTo` was rejected. `replyTo` is absent when the message could not be read at all.
 */
export interface ErrorMessage {
  v: ProtocolVersion;
  type: 'error';
  replyTo?: MessageId;
  payload: ErrorPayload;
}
export interface ErrorPayload {
  code: ErrorCode;
  /**
   * Technical English text for logs. The client shows its own French text for each code.
   */
  message: string;
  details?: ErrorDetails;
}
export interface ErrorDetails {
  reason: IllegalMoveReason;
}
/**
 * Answer to session.hello. Keep the token in sessionStorage to resume after a reconnection.
 */
export interface WelcomeMessage {
  v: ProtocolVersion;
  type: 'session.welcome';
  payload: WelcomePayload;
}
export interface WelcomePayload {
  sessionToken: SessionToken;
  playerId: PlayerId;
  resumedRoomCode?: RoomCode;
}
export interface RoomUpdateMessage {
  v: ProtocolVersion;
  type: 'room.update';
  payload: RoomUpdatePayload;
}
export interface RoomUpdatePayload {
  roomVersion: number;
  room: RoomView;
}
/**
 * State of a room as shown in the lobby. Identical for every member.
 */
export interface RoomView {
  code: RoomCode;
  /**
   * lobby: waiting for players; inGame: a match is running; matchOver: waiting for a rematch.
   */
  phase: 'lobby' | 'inGame' | 'matchOver';
  settings: RoomSettings;
  /**
   * @maxItems 10
   */
  players: RoomMember[];
}
/**
 * House rules chosen by the host (see SPEC §4).
 */
export interface RoomSettings {
  stacking: PenaltyStacking;
  jumpIn: boolean;
  sevenZero: boolean;
  drawAmount: DrawAmount;
  wildDrawFourMode: WildDrawFourMode;
  turnTimerSeconds: TurnTimerSeconds;
  matchLength: MatchLength;
  maxPlayers: MaxPlayers;
  drawRule: DrawRule;
  /**
   * House rule (ADR 0019): the last card cannot be played before UNO is announced.
   */
  declareUnoToWin: boolean;
  drawTwoMultiplier: CardMultiplier3;
  wildDrawFourMultiplier: CardMultiplier4;
  wildDrawFiveMultiplier: CardMultiplier5;
}
export interface RoomMember {
  playerId: PlayerId;
  nickname: string;
  seat: number;
  isHost: boolean;
  isReady: boolean;
  isConnected: boolean;
  isBot: boolean;
}
/**
 * Sent to every player after each accepted action: the events to animate, then the full view to apply. The state version lives in view.stateVersion.
 */
export interface GameUpdateMessage {
  v: ProtocolVersion;
  type: 'game.update';
  payload: GameUpdatePayload;
}
/**
 * serverTime is the server clock when the message was sent: it lets the client correct its own clock offset for turnDeadline and nextRoundDeadline.
 */
export interface GameUpdatePayload {
  serverTime: EpochMillis;
  events: ClientEvent[];
  view: PlayerView;
}
/**
 * A new round was dealt.
 */
export interface RoundStartedEvent {
  kind: 'roundStarted';
  round: number;
  dealerId: PlayerId;
}
/**
 * A card was put on the discard pile. isJumpIn: played out of turn (jumpIn rule).
 */
export interface CardPlayedEvent {
  kind: 'cardPlayed';
  playerId: PlayerId;
  card: Card;
  chosenColor: Color | null;
  isJumpIn: boolean;
}
/**
 * A card. Wild cards have no color of their own (`color: null`).
 */
export interface Card {
  id: CardId;
  color: Color | null;
  rank: Rank;
}
/**
 * A player drew cards. `cards` is present ONLY in the projection sent to the player who drew.
 */
export interface CardsDrawnEvent {
  kind: 'cardsDrawn';
  playerId: PlayerId;
  count: number;
  cards?: Card[];
}
/**
 * It is now this player's turn.
 */
export interface TurnChangedEvent {
  kind: 'turnChanged';
  playerId: PlayerId;
}
/**
 * This player loses their turn (Skip, draw penalty, Reverse with 2 players).
 */
export interface PlayerSkippedEvent {
  kind: 'playerSkipped';
  playerId: PlayerId;
}
/**
 * A Reverse changed the direction of play.
 */
export interface DirectionChangedEvent {
  kind: 'directionChanged';
  direction: Direction;
}
/**
 * The current color was chosen (Wild).
 */
export interface ColorChosenEvent {
  kind: 'colorChosen';
  playerId: PlayerId;
  color: Color;
}
/**
 * A draw card was stacked on a pending penalty.
 */
export interface PenaltyStackedEvent {
  kind: 'penaltyStacked';
  playerId: PlayerId;
  pendingDraw: number;
}
/**
 * Verdict of a Wild Draw Four challenge. `revealedHand` (hand of the challenged player when they played) is present ONLY in the projection sent to the challenger.
 */
export interface ChallengeResolvedEvent {
  kind: 'challengeResolved';
  challengerId: PlayerId;
  challengedId: PlayerId;
  wasBluff: boolean;
  penalizedPlayerId: PlayerId;
  penaltyAmount: number;
  revealedHand?: Card[];
}
/**
 * A Wild Draw Five was played or answered (ADR 0028): `targetId` must draw `total` cards, or answer with another Wild Draw Five. Public: everybody sees who is targeted and how much is at stake.
 */
export interface PlusFiveTargetedEvent {
  kind: 'plusFiveTargeted';
  playerId: PlayerId;
  targetId: PlayerId;
  total: number;
}
/**
 * A player announced UNO.
 */
export interface UnoCalledEvent {
  kind: 'unoCalled';
  playerId: PlayerId;
}
/**
 * A player was caught without announcing UNO and draws a penalty.
 */
export interface UnoCaughtEvent {
  kind: 'unoCaught';
  catcherId: PlayerId;
  targetId: PlayerId;
  penaltyAmount: number;
}
/**
 * sevenZero rule: a 7 swapped two hands.
 */
export interface HandsSwappedEvent {
  kind: 'handsSwapped';
  playerId: PlayerId;
  targetId: PlayerId;
}
/**
 * sevenZero rule: a 0 passed every hand to the next player in this direction.
 */
export interface HandsRotatedEvent {
  kind: 'handsRotated';
  direction: Direction;
}
/**
 * The discard pile (except its top card) was shuffled into a new draw pile.
 */
export interface DeckReshuffledEvent {
  kind: 'deckReshuffled';
  drawPileCount: number;
}
/**
 * A player emptied their hand.
 */
export interface RoundEndedEvent {
  kind: 'roundEnded';
  winnerId: PlayerId;
  points: number;
}
/**
 * The match is over.
 */
export interface MatchEndedEvent {
  kind: 'matchEnded';
  winnerId: PlayerId;
}
/**
 * A player lost their connection (grace period running).
 */
export interface PlayerDisconnectedEvent {
  kind: 'playerDisconnected';
  playerId: PlayerId;
}
/**
 * A player came back.
 */
export interface PlayerReconnectedEvent {
  kind: 'playerReconnected';
  playerId: PlayerId;
}
/**
 * The host role moved to another player.
 */
export interface HostChangedEvent {
  kind: 'hostChanged';
  playerId: PlayerId;
}
/**
 * Projection of a round for ONE player: the only game state ever sent to a client. It never contains the other players' cards, the draw pile order or the random seed. Everything the client may do is precomputed by the server (playableCardIds, canDraw...).
 */
export interface PlayerView {
  /**
   * Increases by 1 with every accepted action. A gap means a missed update: skip animations and apply the view.
   */
  stateVersion: number;
  phase: GamePhase;
  me: MyState;
  /**
   * @minItems 2
   * @maxItems 10
   */
  players: [SeatView, SeatView, ...SeatView[]];
  currentPlayerId: PlayerId;
  direction: Direction;
  /**
   * null only while the first player chooses the color of a Wild turned over at the start of the round.
   */
  currentColor: Color | null;
  discardTop: Card;
  drawPileCount: number;
  /**
   * Accumulated draw penalty waiting for an answer (0 if none).
   */
  pendingDraw: number;
  /**
   * End of the current turn (server clock), null without a turn timer.
   */
  turnDeadline: EpochMillis | null;
  /**
   * In phase roundOver: when the next round starts automatically (server clock).
   */
  nextRoundDeadline: EpochMillis | null;
  actionsOpenAt: EpochMillis1;
  /**
   * Pace of drawn cards, set by the server (ADR 0026): every card takes at least this long to show. The forced move that follows a draw and the clock of a new turn wait for N cards x drawStepMs, so a client paces its animations on this value, never on a constant of its own.
   */
  drawStepMs: number;
  /**
   * Players holding one unannounced card, oldest first (ADR 0018). Only the target may announce until graceEndsAt; from then until expiresAt anybody else may catch them. Server clock.
   */
  unoWindows: UnoWindow[];
  round: number;
  settings: RoomSettings;
  roundResult: RoundResult | null;
  /**
   * Set in phase matchOver.
   */
  matchWinnerId: PlayerId | null;
}
/**
 * What the viewer holds and may do right now.
 */
export interface MyState {
  playerId: PlayerId;
  hand: Card[];
  playableCardIds: CardId[];
  canDraw: boolean;
  /**
   * House rule declareUnoToWin (ADR 0019): the viewer holds one playable card and cannot play it until they announce UNO (game.callUno). Computed by the server.
   */
  mustDeclareUno: boolean;
  /**
   * Pass: keep the card just drawn instead of playing it. Computed by the server from the draw rule (ADR 0017).
   */
  canKeepDrawnCard: boolean;
  canCallUno: boolean;
  canChooseColor: boolean;
  penaltyResponse: PenaltyResponseOptions | null;
}
/**
 * Present when the viewer is targeted by a pending draw penalty.
 */
export interface PenaltyResponseOptions {
  amount: number;
  canChallenge: boolean;
  canStack: boolean;
}
/**
 * Public information about one player (the viewer included).
 */
export interface SeatView {
  playerId: PlayerId;
  nickname: string;
  seat: number;
  cardCount: number;
  score: number;
  isConnected: boolean;
  isBot: boolean;
  isHost: boolean;
  hasCalledUno: boolean;
  isReadyForNextRound: boolean;
}
export interface UnoWindow {
  targetId: PlayerId;
  graceEndsAt: EpochMillis;
  expiresAt: EpochMillis;
}
export interface RoundResult {
  winnerId: PlayerId;
  points: number;
  /**
   * Remaining hands of every player, by playerId (they are counted in the score).
   */
  revealedHands: {
    [k: string]: Card[] | undefined;
  };
}
export interface ReactionMessage {
  v: ProtocolVersion;
  type: 'reaction';
  payload: ReactionPayload;
}
export interface ReactionPayload {
  playerId: PlayerId;
  emote: Emote;
}
export interface RoomClosedMessage {
  v: ProtocolVersion;
  type: 'room.closed';
  payload: RoomClosedPayload;
}
export interface RoomClosedPayload {
  reason: 'expired' | 'kicked' | 'hostClosed';
}
