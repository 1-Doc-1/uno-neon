import { Component, input } from '@angular/core';

export type ButtonKind = 'primary' | 'danger' | 'neutral' | 'ghost' | 'uno';

/**
 * Bouton du jeu, à poser sur un `<button appButton>`. Le néon n'est qu'ici et sur les cartes : `primary` (vert),
 * `danger` (rouge, destructif) et `uno` (jaune) ; `neutral` et `ghost` restent sobres.
 */
@Component({
  // Composant sur un <button> natif : l'accessibilité du bouton reste celle du navigateur.
  // eslint-disable-next-line @angular-eslint/component-selector
  selector: 'button[appButton]',
  template: '<ng-content />',
  host: { '[class]': '"k-" + kind()' },
  styleUrl: './button.scss',
})
export class Button {
  readonly kind = input<ButtonKind>('neutral');
}
