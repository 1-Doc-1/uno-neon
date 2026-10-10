import { NgTemplateOutlet } from '@angular/common';
import { Component, computed, input } from '@angular/core';
import type { Card, Color, Rank } from '../protocol/generated/protocol';
import { cardLabel, cardTint } from './color-meta';

export type CardState = 'neutral' | 'playable' | 'unplayable';

const CORNER_TEXT: Record<Rank, string> = {
  '0': '0',
  '1': '1',
  '2': '2',
  '3': '3',
  '4': '4',
  '5': '5',
  '6': '6',
  '7': '7',
  '8': '8',
  '9': '9',
  skip: '⊘',
  reverse: '⇄',
  drawTwo: '+2',
  wild: '',
  wildDrawFour: '+4',
  wildDrawFive: '+5',
};

/** Les huit rayons qui entourent le « +5 » d'un Joker doré. */
const GOLD_RAYS: readonly number[] = [0, 45, 90, 135, 180, 225, 270, 315];

let nextGoldId = 0;

/** Face d'une carte : un seul SVG paramétré (SPEC §11.4). La taille vient de `--card-w`. */
@Component({
  selector: 'app-card',
  imports: [NgTemplateOutlet],
  templateUrl: './card.html',
  styleUrl: './card.scss',
  host: {
    '[class]': 'tint()',
    '[class.playable]': 'state() === "playable"',
    '[class.unplayable]': 'state() === "unplayable"',
    '[class.top]': 'top()',
    role: 'img',
    '[attr.aria-label]': 'label()',
  },
})
export class CardFace {
  readonly card = input.required<Card>();
  readonly state = input<CardState>('neutral');
  /** Couleur choisie pour un Joker posé : remplit l'anneau central. */
  readonly chosenColor = input<Color | null>(null);
  /** Carte du dessus de la défausse : son bord brille. */
  readonly top = input(false);

  protected readonly tint = computed(() => cardTint(this.card()));
  protected readonly gold = computed(() => this.card().rank === 'wildDrawFive');
  /** Identifiants propres à cette carte pour son dégradé et son masque (plusieurs cartes dorées peuvent coexister). */
  protected readonly goldId = `gold-${nextGoldId++}`;
  protected readonly goldRays = GOLD_RAYS;
  protected readonly label = computed(() => cardLabel(this.card()));
  protected readonly corner = computed(() => CORNER_TEXT[this.card().rank]);
}
