import { DestroyRef, inject, Injectable, signal } from '@angular/core';
import { DOCUMENT } from '@angular/common';
import type { ClientEvent, Color } from '../../../protocol/generated/protocol';
import {
  EffectSpec,
  INITIAL_PLAN_STATE,
  planEffects,
  PlanState,
  REDUCED_MS,
  timingOf,
} from './effect-plan';
import { CATCH_UP_MIN_FACTOR, CATCH_UP_THRESHOLD, MOTION_MS } from '../../../ui/motion';

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
  /** Écart entre deux cartes d'une même pioche : celui du serveur, jamais accéléré par le rattrapage ni la vitesse. */
  readonly staggerMs: number;
  /** Durée du vol d'une carte piochée, déjà ramenée à la vitesse choisie. */
  readonly flightMs: number;
  readonly reduced: boolean;
}

export interface EnqueueOptions {
  /** Vue complète (reconnexion, retard de plusieurs mises à jour) : rien à raconter, on affiche l'état final. */
  readonly resync: boolean;
  /** La couleur active avant ces événements : un Joker la retient le temps de la roue. */
  readonly previousColor: Color | null;
  /** Le rythme des cartes piochées, fixé par le serveur (`PlayerView.drawStepMs`) : une seule source de vérité. */
  readonly drawStepMs: number;
}

interface QueuedStep {
  readonly spec: EffectSpec;
  readonly stepMs: number;
  readonly visibleMs: number;
  readonly drawStepMs: number;
}

/**
 * Le chef d'orchestre des animations : consomme les événements du serveur dans l'ordre et lance les effets l'un après
 * l'autre (les petits se chevauchent). Purement cosmétique : rien ici ne retarde une action ni ne bloque la saisie, et
 * l'état affiché vient toujours de la dernière vue ; au moindre retard ou à la moindre vue complète, on vide la file.
 */
@Injectable()
export class AnimationDirector {
  private readonly document = inject(DOCUMENT);
  private readonly timers = new Set<ReturnType<typeof setTimeout>>();
  private queue: QueuedStep[] = [];
  private running = false;
  private nextId = 1;
  /** Cartes déjà arrivées de chaque pioche en cours (par identifiant d'effet). */
  private readonly arrived = new Map<number, number>();
  private planState: PlanState = INITIAL_PLAN_STATE;

  private readonly activeState = signal<readonly ActiveEffect[]>([]);
  private readonly hiddenState = signal<ReadonlySet<number>>(new Set());
  private readonly heldColorState = signal<Color | null>(null);
  private readonly dialogsHeldState = signal(false);
  private readonly unarrivedState = signal<ReadonlyMap<string, number>>(new Map());
  private readonly unlaunchedState = signal(0);
  /** Cartes piochées déjà dans la vue mais pas encore arrivées, par joueur : les compteurs les retiennent. */
  readonly unarrived = this.unarrivedState.asReadonly();
  /** Cartes de la pioche déjà retirées de la vue mais pas encore parties : le compteur du paquet les garde. */
  readonly unlaunched = this.unlaunchedState.asReadonly();

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
    const steps = specs.map((spec) => ({
      spec,
      ...timingOf(spec, reduced, options.drawStepMs),
      drawStepMs: options.drawStepMs,
    }));
    if (steps.length === 0) {
      return;
    }
    // Retard mesuré à vitesse normale : ralentir les effets (démo) ne doit pas faire abandonner la file.
    // Une pioche rythmée par le serveur n'est jamais abandonnée : le serveur attend sa fin pour la suite.
    const backlog = [...this.queue, ...steps]
      .filter((step) => step.spec.kind !== 'draw')
      .reduce((total, step) => total + step.stepMs, 0);
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
    this.unarrivedState.set(new Map());
    this.unlaunchedState.set(0);
    this.arrived.clear();
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
    for (const { spec } of steps) {
      if (spec.kind === 'draw') {
        this.unarrivedState.update((held) => withDelta(held, spec.playerId, spec.count));
        this.unlaunchedState.update((count) => count + spec.count);
      }
    }
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
        return (spec.cards ?? []).map((card) => card.id);
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
    // Vitesse de la démo de développement, et rattrapage si trop d'étapes attendent
    const factor = this.timeFactor();
    const reduced = this.reduced();
    const { spec } = step;
    const drawing = spec.kind === 'draw';
    // Une pioche garde le rythme du serveur : seul le vol des cartes suit la vitesse de la démo, jamais l'écart entre elles
    const staggerMs = step.drawStepMs / this.speed();
    const flightMs = reduced ? REDUCED_MS : MOTION_MS.drawFlight / this.speed();
    const visibleMs = drawing ? (spec.count - 1) * staggerMs + flightMs : step.visibleMs * factor;
    const effect: ActiveEffect = {
      id: this.nextId++,
      spec,
      durationMs: visibleMs,
      speed: 1 / factor,
      staggerMs,
      flightMs,
      reduced,
    };
    this.activeState.update((list) => [...list, effect]);
    this.after(visibleMs, () => this.finish(effect));
    if (drawing) {
      // Chaque carte quitte le paquet puis arrive à son heure : c'est là que les compteurs montent
      for (let index = 0; index < spec.count; index++) {
        this.after(index * staggerMs, () => this.unlaunchedState.update((count) => count - 1));
        this.after(index * staggerMs + flightMs, () => this.arrive(effect, index));
      }
    }
    if (step.spec.kind === 'wheel') {
      this.after(visibleMs * WHEEL_REVEAL_FRACTION, () => this.heldColorState.set(null));
    }
    const stepMs = drawing ? spec.count * staggerMs : step.stepMs * factor;
    if (stepMs <= 0) {
      this.startNext();
    } else {
      this.after(stepMs, () => this.startNext());
    }
  }

  /** Multiplicateur des durées : la vitesse de la démo, et un rattrapage quand la file s'allonge. */
  private timeFactor(): number {
    const waiting = this.queue.length;
    const catchUp =
      waiting > CATCH_UP_THRESHOLD
        ? Math.max(CATCH_UP_MIN_FACTOR, CATCH_UP_THRESHOLD / waiting)
        : 1;
    return catchUp / this.speed();
  }

  private finish(effect: ActiveEffect): void {
    this.activeState.update((list) => list.filter((other) => other.id !== effect.id));
    const { spec } = effect;
    if (spec.kind === 'draw') {
      // Les cartes arrivées à leur heure sont déjà comptées ; ce qui reste (file vidée) se révèle ici
      for (let index = this.arrived.get(effect.id) ?? 0; index < spec.count; index++) {
        this.arrive(effect, index);
      }
      this.arrived.delete(effect.id);
    } else {
      const ids = this.flyingCardIds(spec);
      if (ids.length > 0) {
        this.hiddenState.update((hidden) => new Set([...hidden].filter((id) => !ids.includes(id))));
      }
    }
    if (effect.spec.kind === 'spotlight') {
      this.dialogsHeldState.set(false);
    }
  }

  /** La carte `index` d'une pioche touche au but : elle apparaît dans la main, le compteur du joueur monte d'un cran. */
  private arrive(effect: ActiveEffect, index: number): void {
    const { spec } = effect;
    if (spec.kind !== 'draw' || (this.arrived.get(effect.id) ?? 0) > index) {
      return;
    }
    this.arrived.set(effect.id, index + 1);
    this.unarrivedState.update((held) => withDelta(held, spec.playerId, -1));
    const card = spec.cards?.[index];
    if (card) {
      this.hiddenState.update((hidden) => new Set([...hidden].filter((id) => id !== card.id)));
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

function withDelta(
  counts: ReadonlyMap<string, number>,
  playerId: string,
  delta: number,
): ReadonlyMap<string, number> {
  const next = new Map(counts);
  const value = Math.max(0, (next.get(playerId) ?? 0) + delta);
  if (value === 0) {
    next.delete(playerId);
  } else {
    next.set(playerId, value);
  }
  return next;
}
