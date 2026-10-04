import { Component, computed, DestroyRef, inject, signal } from '@angular/core';
import { playedCards } from '../features/table/fx/discard-pile';
import { TableView } from '../features/table/table-view';
import type { ClientEvent } from '../protocol/generated/protocol';
import type { EventBatch } from '../state/game-store';
import { buildDemoFrames, DemoFrame } from './animation-script';

const SPEEDS = [0.5, 1, 2] as const;
const FRAME_GAP_MS = 5000;
const LOOP_PAUSE_MS = 3000;

/**
 * `/dev/table?scenario=animations` : rejoue en boucle une séquence scriptée de tous les effets de jeu, à travers le
 * vrai chemin de production (une vue, un lot d'événements, le `AnimationDirector`). Développement seulement.
 */
@Component({
  selector: 'app-animation-demo',
  imports: [TableView],
  template: `
    <app-table-view
      [view]="frame().view"
      [batch]="batch()"
      [played]="played()"
      [animationSpeed]="speed()"
    />
    <div class="controls panel" role="group" aria-label="Démo des animations">
      <button type="button" class="replay" (click)="replay()">Rejouer</button>
      <label>
        Vitesse
        <select (change)="setSpeed($event)">
          @for (option of speeds; track option) {
            <option [value]="option" [selected]="option === speed()">{{ option }}×</option>
          }
        </select>
      </label>
      <p class="caption" role="status">{{ index() }}/{{ total }} — {{ frame().caption }}</p>
    </div>
  `,
  styles: `
    .controls {
      position: fixed;
      top: 34%;
      left: var(--space-3);
      z-index: 20;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: var(--space-3);
      width: min(250px, calc(100vw - 2 * var(--space-3)));
      padding: var(--space-3) var(--space-4);
    }
    .replay {
      min-height: 40px;
      padding: 0 var(--space-4);
      border: 0;
      border-radius: var(--radius-pill);
      background: var(--primary);
      color: var(--text-on-neon);
      font-family: var(--font-display);
      font-weight: 700;
      cursor: pointer;
    }
    select {
      margin-left: var(--space-2);
      padding: var(--space-1) var(--space-2);
      border-radius: var(--radius-sm);
      border: 1px solid var(--text-dim);
      background: var(--field);
      color: var(--text);
    }
    .caption {
      flex-basis: 100%;
      font-size: var(--fs-sm);
    }
  `,
})
export class AnimationDemo {
  private readonly frames = buildDemoFrames(Date.now());
  private timer: ReturnType<typeof setTimeout> | undefined;
  private events: readonly ClientEvent[] = [];
  private batchId = 1;

  protected readonly speeds = SPEEDS;
  protected readonly total = this.frames.length - 1;
  protected readonly index = signal(0);
  protected readonly speed = signal<number>(1);
  protected readonly batch = signal<EventBatch | null>(null);
  protected readonly played = signal<ReturnType<typeof playedCards>>([]);
  protected readonly frame = computed<DemoFrame>(() => this.frames[this.index()]);

  constructor() {
    inject(DestroyRef).onDestroy(() => clearTimeout(this.timer));
    this.replay();
  }

  protected replay(): void {
    clearTimeout(this.timer);
    this.events = [];
    this.show(0, true);
  }

  protected setSpeed(event: Event): void {
    this.speed.set(Number((event.target as HTMLSelectElement).value));
  }

  /** Montre l'étape `index` (la première est une vue complète, sans rien à rejouer), puis programme la suivante. */
  private show(index: number, resync: boolean): void {
    const frame = this.frames[index];
    this.events = resync ? [] : [...this.events, ...frame.events];
    this.played.set(playedCards(this.events));
    this.index.set(index);
    this.batch.set({ id: this.batchId++, events: frame.events, resync });
    const last = index === this.frames.length - 1;
    this.timer = setTimeout(
      () => (last ? this.replay() : this.show(index + 1, false)),
      (last ? LOOP_PAUSE_MS : FRAME_GAP_MS) / this.speed(),
    );
  }
}
