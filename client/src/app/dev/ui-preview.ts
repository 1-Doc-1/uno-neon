import { Component, inject } from '@angular/core';
import { ThemeService, THEMES } from '../core/theme.service';
import type { Card, Color, Rank } from '../protocol/generated/protocol';
import { Button } from '../ui/button';
import { CardFace } from '../ui/card';
import { CardBack } from '../ui/card-back';
import { COLORS, COLOR_NAME } from '../ui/color-meta';
import { ColorSymbol } from '../ui/color-symbol';
import { Icon } from '../ui/icon';
import { Logo } from '../ui/logo';

const NUMBER_RANKS: Rank[] = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9'];
const ACTION_RANKS: Rank[] = ['skip', 'reverse', 'drawTwo'];

let nextId = 0;
const card = (color: Color | null, rank: Rank): Card => ({ id: nextId++, color, rank });

/** Page `/dev` : toutes les cartes, le logo, les boutons et les icônes, pour les revues visuelles. */
@Component({
  selector: 'app-ui-preview',
  imports: [CardFace, CardBack, ColorSymbol, Button, Icon, Logo],
  templateUrl: './ui-preview.html',
  styleUrl: './ui-preview.scss',
})
export class UiPreview {
  protected readonly theme = inject(ThemeService);
  protected readonly themes = THEMES;
  protected readonly colors = COLORS;
  protected readonly colorName = COLOR_NAME;
  protected readonly icons = [
    'door',
    'crown',
    'copy',
    'check',
    'close',
    'arrow-cw',
    'arrow-ccw',
    'palette',
  ] as const;
  protected readonly rows = COLORS.map((color) => ({
    color,
    cards: [...NUMBER_RANKS, ...ACTION_RANKS].map((rank) => card(color, rank)),
  }));
  protected readonly wilds = [card(null, 'wild'), card(null, 'wildDrawFour')];
}
