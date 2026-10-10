import { Component, computed, inject, input } from '@angular/core';
import { AudioService } from '../audio/audio.service';
import { Icon } from './icon';

/**
 * « Son activé / coupé » et le volume (SPEC §13) : visible sur l'accueil et sur la table. Réglage personnel de chaque
 * joueur, gardé par `AudioService`. Un geste sur ce bouton est aussi la première interaction qui débloque l'audio.
 */
@Component({
  selector: 'app-sound-control',
  imports: [Icon],
  template: `
    <div class="sound">
      <button type="button" class="toggle" (click)="audio.setEnabled(!audio.enabled())">
        <app-icon [name]="audio.enabled() ? 'sound' : 'mute'" />
        <span [class.sr-only]="compact()">{{ audio.enabled() ? 'Son activé' : 'Son coupé' }}</span>
      </button>
      @if (audio.enabled() && !compact()) {
        <input
          type="range"
          min="0"
          max="100"
          step="5"
          aria-label="Volume"
          [value]="percent()"
          (input)="onVolume($event)"
        />
      }
    </div>
  `,
  styles: `
    .sound {
      display: inline-flex;
      align-items: center;
      gap: var(--space-3);
      padding: var(--space-1) var(--space-3);
      border-radius: var(--radius-pill);
      background: var(--field);
      color: var(--text);
    }
    .toggle {
      display: inline-flex;
      align-items: center;
      gap: var(--space-2);
      min-height: 36px;
      padding: 0 var(--space-1);
      border: 0;
      background: transparent;
      color: inherit;
      font: inherit;
      font-size: var(--fs-sm);
      font-weight: 600;
      cursor: pointer;

      &:focus-visible {
        outline: 2px solid var(--focus-ring);
        outline-offset: 2px;
        border-radius: var(--radius-pill);
      }
    }
    input {
      width: 96px;
      accent-color: var(--primary);
    }
  `,
})
export class SoundControl {
  protected readonly audio = inject(AudioService);
  /** Petit écran : seule l'icône reste (le texte devient réservé aux lecteurs d'écran), sans curseur de volume. */
  readonly compact = input(false);
  protected readonly percent = computed(() => Math.round(this.audio.volume() * 100));

  protected onVolume(event: Event): void {
    this.audio.setVolume(Number((event.target as HTMLInputElement).value) / 100);
  }
}
