import { Component, computed, DestroyRef, inject, signal } from '@angular/core';
import { playedCards } from '../features/table/fx/discard-pile';
import { INITIAL_PLAN_STATE, planEffects, timingOf } from '../features/table/fx/effect-plan';
import { TableView } from '../features/table/table-view';
import type { ClientEvent, PlayerView } from '../protocol/generated/protocol';
import type { EventBatch } from '../state/game-store';
import { asSpectator, buildDemoFrames, DemoFrame } from './animation-script';

const SPEEDS = [0.5, 1, 2] as const;
const FRAME_GAP_MS = 5000;
/** Rythme des cartes piochées dans la démo : celui que le serveur annonce dans `drawStepMs` (fixtures). */
const DRAW_STEP_MS = 1000;
const LOOP_PAUSE_MS = 3000;

/** Ce que les clients mettent à montrer ces événements, à vitesse normale : le budget que le serveur annoncerait. */
function presentationMs(events: readonly ClientEvent[]): number {
  return planEffects(events, INITIAL_PLAN_STATE).specs.reduce(
    (total, spec) => total + timingOf(spec, false, DRAW_STEP_MS).stepMs,
    0,
  );
}

/**
 * `/dev/table?scenario=animations` : rejoue en boucle une séquence scriptée de tous les effets de jeu, à travers le
 * vrai chemin de production (une vue, un lot d'événements, le `AnimationDirector`). Développement seulement.
 */
@Component({
  selector: 'app-animation-demo',
  imports: [TableView],
  template: `
    <app-table-view
      [view]="view()"
      [batch]="batch()"
      [played]="played()"
      [animationSpeed]="speed()"
    />
    <div class="controls panel" role="group" aria-label="Démo des animations">
      <button type="button" class="replay" (click)="replay()">Rejouer</button>
      <label>
        Point de vue
        <select (change)="setViewpoint($event)">
          <option value="me" [selected]="viewpoint() === 'me'">Moi (la cible)</option>
          <option value="spectator" [selected]="viewpoint() === 'spectator'">
            Spectateur (Zoé)
          </option>
        </select>
      </label>
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
  private readonly myFrames = buildDemoFrames(Date.now());
  private readonly spectatorFrames = asSpectator(this.myFrames);
  private timer: ReturnType<typeof setTimeout> | undefined;
  private events: readonly ClientEvent[] = [];
  private batchId = 1;

  protected readonly speeds = SPEEDS;
  /** Qui regarde : moi (les effets qui me visent en grand au centre) ou un spectateur (ils se jouent sur le siège de la cible). */
  protected readonly viewpoint = signal<'me' | 'spectator'>('me');
  private readonly frames = computed(() =>
    this.viewpoint() === 'me' ? this.myFrames : this.spectatorFrames,
  );
  protected readonly total = this.myFrames.length - 1;
  protected readonly index = signal(0);
  protected readonly speed = signal<number>(1);
  protected readonly batch = signal<EventBatch | null>(null);
  protected readonly played = signal<ReturnType<typeof playedCards>>([]);
  protected readonly frame = computed<DemoFrame>(() => this.frames()[this.index()]);
  /** Quand la main s'ouvre : le serveur le dit dans chaque vue (ADR 0027), la démo le calcule d'après les effets montrés. */
  private readonly actionsOpenAt = signal(0);
  protected readonly view = computed<PlayerView>(() => ({
    ...this.frame().view,
    actionsOpenAt: this.actionsOpenAt(),
  }));

  constructor() {
    inject(DestroyRef).onDestroy(() => clearTimeout(this.timer));
    this.replay();
  }

  protected replay(): void {
    clearTimeout(this.timer);
    this.events = [];
    this.show(0, true);
  }

  protected setViewpoint(event: Event): void {
    this.viewpoint.set(
      (event.target as HTMLSelectElement).value === 'spectator' ? 'spectator' : 'me',
    );
    this.replay();
  }

  protected setSpeed(event: Event): void {
    this.speed.set(Number((event.target as HTMLSelectElement).value));
  }

  /** Montre l'étape `index` (la première est une vue complète, sans rien à rejouer), puis programme la suivante. */
  private show(index: number, resync: boolean): void {
    const frame = this.frames()[index];
    this.events = resync ? [] : [...this.events, ...frame.events];
    this.played.set(playedCards(this.events));
    this.index.set(index);
    this.actionsOpenAt.set(resync ? 0 : Date.now() + presentationMs(frame.events) / this.speed());
    this.batch.set({ id: this.batchId++, events: frame.events, resync });
    const last = index === this.frames().length - 1;
    // Une pioche rythmée dure une seconde par carte : la démo laisse le temps de la voir en entier
    const drawn = frame.events.filter((event) => event.kind === 'cardsDrawn').length;
    this.timer = setTimeout(
      () => (last ? this.replay() : this.show(index + 1, false)),
      ((last ? LOOP_PAUSE_MS : FRAME_GAP_MS) + drawn * DRAW_STEP_MS) / this.speed(),
    );
  }
}
