import { Component, computed, inject, input } from '@angular/core';
import { AudioService } from '../audio/audio.service';
import { ClickSound } from './click-sound';
import { Icon } from './icon';

/**
 * Les réglages audio (SPEC §13.3) : « Son activé / coupé » (le muet général) toujours visible, et dessous un panneau
 * « Réglages audio » (volume des effets, volume de la musique, tension de fin de tour). Visible sur l'accueil et sur la
 * table. Réglages personnels de chaque joueur, gardés par `AudioService`.
 */
@Component({
  selector: 'app-sound-control',
  imports: [ClickSound, Icon],
  template: `
    <div class="sound">
      <button type="button" class="toggle" (click)="audio.setEnabled(!audio.enabled())">
        <app-icon [name]="audio.enabled() ? 'sound' : 'mute'" />
        <span [class.sr-only]="compact()">{{ audio.enabled() ? 'Son activé' : 'Son coupé' }}</span>
      </button>
      @if (audio.enabled()) {
        <details class="more">
          <summary>
            <span [class.sr-only]="compact()">Réglages audio</span>
            <span class="gear" aria-hidden="true">⚙</span>
          </summary>
          <div class="panel settings">
            <label>
              <span>Effets</span>
              <input
                type="range"
                min="0"
                max="100"
                step="5"
                aria-label="Volume des effets"
                [value]="effectsPercent()"
                (input)="onEffects($event)"
              />
            </label>
            <label>
              <span>Musique</span>
              <input
                type="range"
                min="0"
                max="100"
                step="5"
                aria-label="Volume de la musique"
                [value]="musicPercent()"
                (input)="onMusic($event)"
              />
            </label>
            <label class="check">
              <input
                type="checkbox"
                [checked]="audio.tensionEnabled()"
                (change)="onTension($event)"
              />
              <span>Tension de fin de tour</span>
            </label>
          </div>
        </details>
      }
    </div>
  `,
  styles: `
    .sound {
      position: relative;
      display: inline-flex;
      align-items: center;
      gap: var(--space-3);
      padding: var(--space-1) var(--space-3);
      border-radius: var(--radius-pill);
      background: var(--field);
      color: var(--text);
    }
    .toggle,
    summary {
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
      list-style: none;

      &:focus-visible {
        outline: 2px solid var(--focus-ring);
        outline-offset: 2px;
        border-radius: var(--radius-pill);
      }
    }
    summary::-webkit-details-marker {
      display: none;
    }
    .settings {
      position: absolute;
      top: calc(100% + var(--space-2));
      right: 0;
      z-index: var(--z-table);
      display: grid;
      gap: var(--space-3);
      width: max-content;
      min-width: 240px;
      padding: var(--space-4);
      font-size: var(--fs-sm);
    }
    label {
      display: grid;
      grid-template-columns: 5rem 1fr;
      align-items: center;
      gap: var(--space-3);
    }
    .check {
      grid-template-columns: auto 1fr;
    }
    input[type='range'] {
      width: 120px;
      accent-color: var(--primary);
    }
    input[type='checkbox'] {
      accent-color: var(--primary);
      width: 18px;
      height: 18px;
    }
  `,
})
export class SoundControl {
  protected readonly audio = inject(AudioService);
  /** Petit écran : seules les icônes restent (le texte devient réservé aux lecteurs d'écran). */
  readonly compact = input(false);
  protected readonly effectsPercent = computed(() => Math.round(this.audio.volume() * 100));
  protected readonly musicPercent = computed(() => Math.round(this.audio.musicVolume() * 100));

  protected onEffects(event: Event): void {
    this.audio.setVolume(percentOf(event));
  }

  protected onMusic(event: Event): void {
    this.audio.setMusicVolume(percentOf(event));
  }

  protected onTension(event: Event): void {
    this.audio.setTensionEnabled((event.target as HTMLInputElement).checked);
  }
}

function percentOf(event: Event): number {
  return Number((event.target as HTMLInputElement).value) / 100;
}
