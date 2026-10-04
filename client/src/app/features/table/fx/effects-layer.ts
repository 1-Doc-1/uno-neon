import {
  afterNextRender,
  Component,
  effect,
  ElementRef,
  inject,
  Injector,
  input,
  signal,
  untracked,
} from '@angular/core';
import { CardFace } from '../../../ui/card';
import { CardBack } from '../../../ui/card-back';
import { COLORS } from '../../../ui/color-meta';
import type { ActiveEffect } from './animation-director';
import { Anchors, Box, placeEffect, Placed } from './effect-geometry';
import { FlightMotion } from './flight-motion';
import { ReverseArrow } from './reverse-arrow';

/** Une étoile à seize branches pour l'éclat « UNO ! » (rayons alternés, centrée en 0,0). */
const STAR = Array.from({ length: 16 }, (_, index) => {
  const angle = (index * Math.PI) / 8;
  const radius = index % 2 === 0 ? 48 : 30;
  return `${(radius * Math.sin(angle)).toFixed(1)},${(-radius * Math.cos(angle)).toFixed(1)}`;
}).join(' ');

/** Quarts de la roue des couleurs, dans l'ordre de `COLORS` (rouge, jaune, vert, bleu), en partant du haut à droite. */
const QUARTERS: Record<string, string> = {
  red: 'M0 0 L0 -46 A46 46 0 0 1 46 0 Z',
  yellow: 'M0 0 L46 0 A46 46 0 0 1 0 46 Z',
  green: 'M0 0 L0 46 A46 46 0 0 1 -46 0 Z',
  blue: 'M0 0 L-46 0 A46 46 0 0 1 0 -46 Z',
};

const SHAKE = [0, -7, 7, -5, 5, -2, 0].map((x) => ({ translate: `${x}px` }));
const BLINK = [1, 0.35, 1, 0.35, 1].map((opacity) => ({ opacity }));

/**
 * La couche des effets : par-dessus la table, sans jamais intercepter un clic, elle montre les effets que le
 * `AnimationDirector` lance (cartes qui volent, gros « +2 », roue des couleurs, tampons…). Chaque effet est placé une
 * seule fois, à son démarrage, d'après la position réelle des éléments marqués `data-anchor` de la table.
 */
@Component({
  selector: 'app-effects-layer',
  imports: [CardFace, CardBack, FlightMotion, ReverseArrow],
  templateUrl: './effects-layer.html',
  styleUrl: './effects-layer.scss',
  host: { 'aria-hidden': 'true' },
})
export class EffectsLayer {
  readonly effects = input.required<readonly ActiveEffect[]>();
  readonly meId = input.required<string>();
  /** Les pseudos par identifiant : les effets vus par les autres nomment leur cible. */
  readonly names = input<Readonly<Record<string, string>>>({});
  /** La dernière position connue d'une carte de ma main (elle a peut-être déjà quitté l'écran). */
  readonly handRect = input<(cardId: number) => DOMRect | null>(() => null);

  private readonly host: HTMLElement = inject(ElementRef).nativeElement;
  private readonly injector = inject(Injector);
  private readonly cache = new Map<number, Placed | null>();
  protected readonly placed = signal<readonly Placed[]>([]);
  protected readonly star = STAR;
  protected readonly quarters = COLORS.map((color) => ({ color, path: QUARTERS[color] }));

  constructor() {
    effect(() => {
      const active = this.effects();
      // Après le rendu : la carte qui vient d'arriver dans ma main doit avoir sa place avant qu'on vise son arrivée
      untracked(() => afterNextRender(() => this.sync(active), { injector: this.injector }));
    });
  }

  protected wheelSize(table: Box): number {
    return Math.min(table.h * 0.75, 240);
  }

  private sync(active: readonly ActiveEffect[]): void {
    const alive = new Set(active.map((effect) => effect.id));
    for (const id of this.cache.keys()) {
      if (!alive.has(id)) {
        this.cache.delete(id);
      }
    }
    const anchors = this.anchors();
    for (const effect of active) {
      if (!this.cache.has(effect.id)) {
        this.cache.set(effect.id, placeEffect(effect, anchors));
        this.hitSeat(effect);
      }
    }
    this.placed.set([...this.cache.values()].filter((placed): placed is Placed => placed !== null));
  }

  private anchors(): Anchors {
    const layer = this.host.getBoundingClientRect();
    const toBox = (rect: DOMRect): Box => ({
      cx: rect.left - layer.left + rect.width / 2,
      cy: rect.top - layer.top + rect.height / 2,
      w: rect.width,
      h: rect.height,
    });
    return {
      meId: this.meId(),
      nameOf: (playerId) => this.names()[playerId] ?? 'Un joueur',
      layer: { cx: layer.width / 2, cy: layer.height / 2, w: layer.width, h: layer.height },
      box: (anchor) => {
        const element = this.anchorElement(anchor);
        return element ? toBox(element.getBoundingClientRect()) : null;
      },
      handCard: (cardId) => {
        const rect = this.handRect()(cardId);
        return rect ? toBox(rect) : null;
      },
    };
  }

  private anchorElement(anchor: string): HTMLElement | null {
    const root = this.host.parentElement ?? this.host;
    return root.querySelector<HTMLElement>(`[data-anchor="${anchor}"]`);
  }

  /** Une pioche infligée fait trembler et clignoter le siège visé, une fois les premières cartes en route. */
  private hitSeat(effect: ActiveEffect): void {
    const { spec } = effect;
    if (spec.kind !== 'draw' || !spec.penalty || effect.reduced) {
      return;
    }
    const seat = this.anchorElement(`seat:${spec.playerId}`);
    if (seat && typeof seat.animate === 'function') {
      const options = { duration: 380 / effect.speed, delay: 160 / effect.speed };
      seat.animate(SHAKE, options);
      seat.animate(BLINK, options);
    }
  }
}
