import { Service } from '@angular/core';

const TOKEN_KEY = 'uno.sessionToken';
const NICKNAME_KEY = 'uno.nickname';

/**
 * Mémoire du navigateur : le jeton de session vit dans `sessionStorage` (un onglet = un joueur, ce qui
 * permet de jouer à deux dans deux onglets), le pseudo dans `localStorage`.
 */
@Service()
export class SessionService {
  token(): string | null {
    return read(() => sessionStorage.getItem(TOKEN_KEY));
  }

  saveToken(token: string): void {
    write(() => sessionStorage.setItem(TOKEN_KEY, token));
  }

  clearToken(): void {
    write(() => sessionStorage.removeItem(TOKEN_KEY));
  }

  nickname(): string {
    return read(() => localStorage.getItem(NICKNAME_KEY)) ?? '';
  }

  saveNickname(nickname: string): void {
    write(() => localStorage.setItem(NICKNAME_KEY, nickname));
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
