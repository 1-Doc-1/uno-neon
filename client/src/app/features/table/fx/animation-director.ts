import { DestroyRef, inject, Injectable, signal } from '@angular/core';
import { DOCUMENT } from '@angular/common';
import type { ClientEvent, Color } from '../../../protocol/generated/protocol';
import {
  EffectSpec,
  flyingCards,
  INITIAL_PLAN_STATE,
  planEffects,
  PlanState,
  timingOf,
} from './effect-plan';
import {
  CATCH_UP_MIN_FACTOR,
  CATCH_UP_THRESHOLD,
  MOTION_MS,
  MotionPreferences,
} from '../../../ui/motion';

/** Au-delà de ce retard cumulé, la file est abandonnée : mieux vaut l'état final que des effets qui n'ont plus de sens. */
export const MAX_BACKLOG_MS = MOTION_MS.maxBacklog;
const REDUCED_QUERY = '(prefers-reduced-motion: reduce)';
/** Le Joker retient la couleur affichée jusqu'à ce point de la roue (part de sa durée), puis le liseré la prend. */
const WHEEL_REVEAL_FRACTION = 0.65;

export interface ActiveEffect {
  readonly id: number;
  readonly spec: EffectSpec;
  /** Durée visible, déjà ramenée à la vitesse choisie. */
  readonly durationMs: number;
  readonly speed: number;
  /** Écart entre deux cartes d'une même pioche, déjà ramené à la vitesse choisie. */
  readonly staggerMs: number;
  readonly reduced: boolean;
}

export interface EnqueueOptions {
  /** Vue complète (reconnexion, retard de plusieurs mises à jour) : rien à raconter, on affiche l'état final. */
  readonly resync: boolean;
  /** La couleur active avant ces événements : un Joker la retient le temps de la roue. */
  readonly previousColor: Color | null;
}

interface QueuedStep {
  readonly spec: EffectSpec;
  readonly stepMs: number;
  readonly visibleMs: number;
}

/**
 * Le chef d'orchestre des animations : consomme les événements du serveur dans l'ordre et lance les effets l'un après
 * l'autre (les petits se chevauchent). Purement cosmétique : rien ici ne retarde une action ni ne bloque la saisie, et
 * l'état affiché vient toujours de la dernière vue ; au moindre retard ou à la moindre vue complète, on vide la file.
 */
@Injectable()
export class AnimationDirector {
  private readonly document = inject(DOCUMENT);
  private readonly motion = inject(MotionPreferences);
  private readonly timers = new Set<ReturnType<typeof setTimeout>>();
  private queue: QueuedStep[] = [];
  private running = false;
  private nextId = 1;
  private planState: PlanState = INITIAL_PLAN_STATE;

  private readonly activeState = signal<readonly ActiveEffect[]>([]);
  private readonly hiddenState = signal<ReadonlySet<number>>(new Set());
  private readonly heldColorState = signal<Color | null>(null);
  private readonly dialogsHeldState = signal(false);

  /** Les effets visibles en ce moment. */
  readonly active = this.activeState.asReadonly();
  /** Cartes de la vue encore « en vol » : présentes dans l'état mais cachées jusqu'à leur arrivée. */
  readonly hiddenCardIds = this.hiddenState.asReadonly();
  /** Couleur à montrer à la place de la couleur active pendant la roue d'un Joker (`null` : celle de la vue). */
  readonly heldColor = this.heldColorState.asReadonly();
  /** Vrai pendant le projecteur de fin de manche : la modale de fin de manche attend. */
  readonly dialogsHeld = this.dialogsHeldState.asReadonly();
  readonly speed = signal(1);
  readonly reduced = signal(false);

  constructor() {
    const view = this.document.defaultView;
    if (view && typeof view.matchMedia === 'function') {
      const query = view.matchMedia(REDUCED_QUERY);
      this.reduced.set(query.matches);
      const update = (): void => this.reduced.set(query.matches);
      query.addEventListener('change', update);
      inject(DestroyRef).onDestroy(() => query.removeEventListener('change', update));
    }
    const onVisibility = (): void => {
      if (this.document.visibilityState === 'hidden') {
        this.flush();
      }
    };
    this.document.addEventListener('visibilitychange', onVisibility);
    inject(DestroyRef).onDestroy(() => {
      this.document.removeEventListener('visibilitychange', onVisibility);
      this.flush();
    });
  }

  /** Ajoute les événements d'une mise à jour à la file. */
  enqueue(events: readonly ClientEvent[], options: EnqueueOptions): void {
    if (options.resync || this.document.visibilityState === 'hidden') {
      this.flush();
      return;
    }
    const { specs, state } = planEffects(events, this.planState);
    this.planState = state;
    const reduced = this.reduced();
    const steps = specs.map((spec) => ({ spec, ...timingOf(spec, reduced) }));
    if (steps.length === 0) {
      return;
    }
    // Retard mesuré à vitesse normale : ralentir les effets (démo) ne doit pas faire abandonner la file
    const backlog = [...this.queue, ...steps].reduce((total, step) => total + step.stepMs, 0);
    if (backlog > MAX_BACKLOG_MS) {
      this.flush();
      return;
    }
    this.hold(steps, options.previousColor);
    this.queue.push(...steps);
    if (!this.running) {
      this.running = true;
      this.startNext();
    }
  }

  /** Abandonne tout : file, effets en cours, cartes cachées, couleur retenue. L'écran montre alors la dernière vue. */
  flush(): void {
    for (const timer of this.timers) {
      clearTimeout(timer);
    }
    this.timers.clear();
    this.queue = [];
    this.running = false;
    this.planState = INITIAL_PLAN_STATE;
    this.activeState.set([]);
    this.hiddenState.set(new Set());
    this.heldColorState.set(null);
    this.dialogsHeldState.set(false);
  }

  /** Cache dès maintenant ce qui va voler (sinon la vue montrerait la carte avant son départ) et retient ce qui doit attendre. */
  private hold(steps: readonly QueuedStep[], previousColor: Color | null): void {
    const hidden = new Set(this.hiddenState());
    for (const { spec } of steps) {
      for (const id of this.flyingCardIds(spec)) {
        hidden.add(id);
      }
    }
    this.hiddenState.set(hidden);
    if (this.heldColorState() === null && steps.some(({ spec }) => spec.kind === 'wheel')) {
      this.heldColorState.set(previousColor);
    }
    if (steps.some(({ spec }) => spec.kind === 'spotlight')) {
      this.dialogsHeldState.set(true);
    }
  }

  private flyingCardIds(spec: EffectSpec): readonly number[] {
    switch (spec.kind) {
      case 'play':
        return [spec.card.id];
      case 'draw':
        return (spec.cards ?? []).slice(0, flyingCards(spec.count)).map((card) => card.id);
      default:
        return [];
    }
  }

  private startNext(): void {
    const step = this.queue.shift();
    if (!step) {
      this.running = false;
      return;
    }
    // Réglage du joueur (Normale/Rapide), vitesse de la démo, et rattrapage si trop d'étapes attendent
    const factor = this.timeFactor();
    const reduced = this.reduced();
    const visibleMs = step.visibleMs * factor;
    const effect: ActiveEffect = {
      id: this.nextId++,
      spec: step.spec,
      durationMs: visibleMs,
      speed: 1 / factor,
      staggerMs: MOTION_MS.drawStagger * factor,
      reduced,
    };
    this.activeState.update((list) => [...list, effect]);
    this.after(visibleMs, () => this.finish(effect));
    if (step.spec.kind === 'wheel') {
      this.after(visibleMs * WHEEL_REVEAL_FRACTION, () => this.heldColorState.set(null));
    }
    if (step.stepMs <= 0) {
      this.startNext();
    } else {
      this.after(step.stepMs * factor, () => this.startNext());
    }
  }

  /** Multiplicateur des durées : le réglage du joueur, la vitesse de la démo, et un rattrapage quand la file s'allonge. */
  private timeFactor(): number {
    const waiting = this.queue.length;
    const catchUp =
      waiting > CATCH_UP_THRESHOLD
        ? Math.max(CATCH_UP_MIN_FACTOR, CATCH_UP_THRESHOLD / waiting)
        : 1;
    return (this.motion.scale() / this.speed()) * catchUp;
  }

  private finish(effect: ActiveEffect): void {
    this.activeState.update((list) => list.filter((other) => other.id !== effect.id));
    const ids = this.flyingCardIds(effect.spec);
    if (ids.length > 0) {
      this.hiddenState.update((hidden) => new Set([...hidden].filter((id) => !ids.includes(id))));
    }
    if (effect.spec.kind === 'spotlight') {
      this.dialogsHeldState.set(false);
    }
  }

  private after(delayMs: number, action: () => void): void {
    const timer = setTimeout(() => {
      this.timers.delete(timer);
      action();
    }, delayMs);
    this.timers.add(timer);
  }
}
