import { DOCUMENT } from '@angular/common';
import { effect, inject, Service, signal } from '@angular/core';

export type ThemeName = 'tapis' | 'nuit';

export const THEMES: readonly { readonly name: ThemeName; readonly label: string }[] = [
  { name: 'tapis', label: 'Tapis' },
  { name: 'nuit', label: 'Nuit' },
];

const STORAGE_KEY = 'uno.theme';

function isThemeName(value: unknown): value is ThemeName {
  return THEMES.some((theme) => theme.name === value);
}

/**
 * Thème visuel (SPEC §11) : un attribut `data-theme` sur `<html>` change tout le jeu de tokens. Le choix vient de
 * `?theme=` dans l'adresse (pratique pour les captures), sinon du navigateur, sinon « Tapis ».
 */
@Service()
export class ThemeService {
  private readonly document = inject(DOCUMENT);
  private readonly state = signal<ThemeName>(this.initial());

  readonly theme = this.state.asReadonly();

  constructor() {
    effect(() => {
      const theme = this.state();
      this.document.documentElement.dataset['theme'] = theme;
      try {
        localStorage.setItem(STORAGE_KEY, theme);
      } catch {
        // Stockage indisponible : le thème vaut pour cette visite seulement.
      }
    });
  }

  set(theme: ThemeName): void {
    this.state.set(theme);
  }

  toggle(): void {
    this.state.update((theme) => (theme === 'tapis' ? 'nuit' : 'tapis'));
  }

  private initial(): ThemeName {
    const fromUrl = new URLSearchParams(this.document.location.search).get('theme');
    if (isThemeName(fromUrl)) {
      return fromUrl;
    }
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (isThemeName(stored)) {
        return stored;
      }
    } catch {
      // Stockage indisponible.
    }
    return 'tapis';
  }
}
