import type { ErrorCode, IllegalMoveReason } from '../protocol/generated/protocol';

const MESSAGES: Record<ErrorCode, string> = {
  MALFORMED_MESSAGE: 'Message illisible. Recharge la page si le problème persiste.',
  UNKNOWN_TYPE: 'Action inconnue du serveur.',
  UNSUPPORTED_VERSION: 'Cette version du jeu est trop ancienne : recharge la page.',
  MESSAGE_TOO_LARGE: 'Message trop volumineux.',
  RATE_LIMITED: 'Doucement ! Trop d’actions en peu de temps.',
  SESSION_REQUIRED: 'Connexion en cours, réessaie dans un instant.',
  SESSION_EXPIRED: 'Ta session a expiré : une nouvelle session a été ouverte.',
  NICKNAME_INVALID: 'Pseudo invalide : 2 à 16 lettres, chiffres, espaces, _ ou -.',
  NICKNAME_TAKEN: 'Ce pseudo est déjà pris dans ce salon.',
  ALREADY_IN_ROOM: 'Tu es déjà dans un salon.',
  NOT_IN_ROOM: 'Tu n’es dans aucun salon.',
  ROOM_NOT_FOUND: 'Aucun salon ne porte ce code.',
  ROOM_FULL: 'Ce salon est complet.',
  MATCH_IN_PROGRESS: 'La partie est déjà en cours.',
  NOT_HOST: 'Seul l’hôte peut faire ça.',
  CANNOT_KICK_SELF: 'Tu ne peux pas t’exclure toi-même.',
  INVALID_SETTINGS: 'Ces réglages ne sont pas disponibles.',
  NOT_ENOUGH_PLAYERS: 'Il faut au moins 2 joueurs pour lancer la partie.',
  PLAYERS_NOT_READY: 'Tous les joueurs doivent être prêts.',
  NOT_YOUR_TURN: 'Ce n’est pas ton tour.',
  INVALID_PHASE: 'Action impossible à ce moment de la partie.',
  CARD_NOT_IN_HAND: 'Cette carte n’est pas dans ta main.',
  ILLEGAL_MOVE: 'Coup interdit.',
  UNO_WINDOW_CLOSED: 'Trop tard : on ne peut plus annoncer UNO ni contrer.',
  UNO_GRACE_PERIOD: 'Trop tôt : ce joueur peut encore annoncer UNO.',
};

const ILLEGAL_REASONS: Record<IllegalMoveReason, string> = {
  COLOR_MISMATCH: 'Cette carte n’a ni la couleur ni la valeur de la défausse.',
  WILD_DRAW_FOUR_ILLEGAL: 'Joker +4 interdit : tu as une carte de la couleur courante.',
  COLOR_REQUIRED: 'Choisis une couleur pour ce joker.',
  COLOR_NOT_ALLOWED: 'Cette carte ne demande pas de couleur.',
  SWAP_TARGET_REQUIRED: 'Choisis un joueur avec qui échanger.',
  SWAP_TARGET_INVALID: 'Ce joueur ne peut pas être choisi.',
  ONLY_DRAWN_CARD_PLAYABLE: 'Tu peux seulement jouer la carte que tu viens de piocher.',
  JUMP_IN_TOO_LATE: 'Trop tard pour intercepter.',
  CANNOT_STACK: 'Tu ne peux pas empiler cette carte.',
  CANNOT_CHALLENGE: 'Tu ne peux pas contester ici.',
  MUST_PLAY: 'Tu dois jouer une carte : la pioche n’est pas permise ici.',
  MUST_DECLARE_UNO: 'Annonce UNO avant de poser ta dernière carte.',
};

export function describeError(code: ErrorCode, reason?: IllegalMoveReason): string {
  return reason ? ILLEGAL_REASONS[reason] : MESSAGES[code];
}
