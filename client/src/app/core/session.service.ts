import { Service } from '@angular/core';
import { AVATAR_COUNT } from '../ui/avatar';

const NICKNAME_KEY = 'uno.nickname';
const AVATAR_KEY = 'uno.avatar';

/**
 * Mémoire du navigateur : le pseudo et l'avatar vivent dans `localStorage`. Le jeton de session, lui, est dans un
 * cookie HttpOnly posé par le serveur : aucun script ne le voit (ADR 0036).
 */
@Service()
export class SessionService {
  nickname(): string {
    return read(() => localStorage.getItem(NICKNAME_KEY)) ?? '';
  }

  saveNickname(nickname: string): void {
    write(() => localStorage.setItem(NICKNAME_KEY, nickname));
  }

  /** L'avatar choisi ; le premier jour, un avatar tiré au hasard (et gardé) pour que deux amis n’aient pas le même. */
  avatar(): number {
    const raw = read(() => localStorage.getItem(AVATAR_KEY));
    const saved = raw === null ? Number.NaN : Number(raw);
    if (Number.isInteger(saved) && saved >= 0 && saved < AVATAR_COUNT) {
      return saved;
    }
    const fresh = Math.floor(Math.random() * AVATAR_COUNT);
    this.saveAvatar(fresh);
    return fresh;
  }

  saveAvatar(avatar: number): void {
    write(() => localStorage.setItem(AVATAR_KEY, String(avatar)));
  }
}

function read(action: () => string | null): string | null {
  try {
    return action();
  } catch {
    return null;
  }
}

function write(action: () => void): void {
  try {
    action();
  } catch {
    // Stockage indisponible (navigation privée, politique du navigateur) : on joue sans mémoire.
  }
}
