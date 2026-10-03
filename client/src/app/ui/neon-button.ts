import { Component, input } from '@angular/core';

export type ButtonVariant = 'solid' | 'outline' | 'ghost';
export type ButtonTone = 'cyan' | 'pink' | 'green' | 'yellow' | 'red' | 'violet';

/** Bouton néon : à poser sur un `<button neonButton>`. Texte sombre sur aplat, jamais blanc (SPEC §11.2). */
@Component({
  // Composant sur un <button> natif : l'accessibilité du bouton reste celle du navigateur.
  // eslint-disable-next-line @angular-eslint/component-selector
  selector: 'button[appNeonButton]',
  template: '<ng-content />',
  host: {
    '[class]': '"v-" + variant() + " t-" + tone()',
  },
  styleUrl: './neon-button.scss',
})
export class NeonButton {
  readonly variant = input<ButtonVariant>('solid');
  readonly tone = input<ButtonTone>('cyan');
}
