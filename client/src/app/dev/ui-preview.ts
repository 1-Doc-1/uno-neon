import { Component } from '@angular/core';
import type { Card, Color, Rank } from '../protocol/generated/protocol';
import { CardBack } from '../ui/card-back';
import { CardFace } from '../ui/card';
import { COLORS, COLOR_NAME } from '../ui/color-meta';
import { ColorSymbol } from '../ui/color-symbol';
import { NeonButton } from '../ui/neon-button';

const NUMBER_RANKS: Rank[] = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9'];
const ACTION_RANKS: Rank[] = ['skip', 'reverse', 'drawTwo'];

let nextId = 0;
const card = (color: Color | null, rank: Rank): Card => ({ id: nextId++, color, rank });

/** Page `/dev` : toutes les cartes et leurs états, pour les revues visuelles. */
@Component({
  selector: 'app-ui-preview',
  imports: [CardFace, CardBack, ColorSymbol, NeonButton],
  templateUrl: './ui-preview.html',
  styleUrl: './ui-preview.scss',
})
export class UiPreview {
  protected readonly colors = COLORS;
  protected readonly colorName = COLOR_NAME;
  protected readonly rows = COLORS.map((color) => ({
    color,
    cards: [...NUMBER_RANKS, ...ACTION_RANKS].map((rank) => card(color, rank)),
  }));
  protected readonly wilds = [card(null, 'wild'), card(null, 'wildDrawFour')];
}
