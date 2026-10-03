/** Mêmes règles que le serveur (2 à 16 lettres, chiffres, espace, _ ou -) : il reste l'autorité. */
export const NICKNAME_PATTERN = /^\s*[\p{L}\p{N} _-]{2,16}\s*$/u;

const ROOM_CODE_PATTERN = /^[ABCDEFGHJKMNPQRSTUVWXYZ23456789]{6}$/;

/** Les codes de salon sont saisis sans tenir compte de la casse. */
export function isRoomCode(text: string): boolean {
  return ROOM_CODE_PATTERN.test(text.trim().toUpperCase());
}
