import { NgTemplateOutlet } from '@angular/common';
import { Component, computed, input } from '@angular/core';
import type { Card, Color, Rank } from '../protocol/generated/protocol';
import { cardLabel, tintClass } from './color-meta';

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
};

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
    role: 'img',
    '[attr.aria-label]': 'label()',
  },
})
export class CardFace {
  readonly card = input.required<Card>();
  readonly state = input<CardState>('neutral');
  /** Couleur choisie pour un Joker posé : remplit l'anneau central. */
  readonly chosenColor = input<Color | null>(null);

  protected readonly tint = computed(() => tintClass(this.card().color));
  protected readonly label = computed(() => cardLabel(this.card()));
  protected readonly corner = computed(() => CORNER_TEXT[this.card().rank]);
}
