import { Component, inject } from '@angular/core';
import { MotionMode, MotionPreferences } from './motion';
import { SegmentOption, Segmented } from './segmented';

const MODES: readonly SegmentOption<MotionMode>[] = [
  { value: 'normal', label: 'Normale' },
  { value: 'fast', label: 'Rapide' },
];

/** « Vitesse des animations : Normale / Rapide », propre à ce navigateur (jamais envoyée au serveur). */
@Component({
  selector: 'app-motion-setting',
  imports: [Segmented],
  template: `
    <span class="label">Vitesse des animations</span>
    <app-segmented
      label="Vitesse des animations"
      [options]="modes"
      [value]="motion.mode()"
      (selected)="motion.set($event)"
    />
  `,
  styles: `
    :host {
      display: inline-flex;
      align-items: center;
      gap: var(--space-2);
      color: var(--text-dim);
      font-size: var(--fs-xs);
    }
  `,
})
export class MotionSetting {
  protected readonly motion = inject(MotionPreferences);
  protected readonly modes = MODES;
}
