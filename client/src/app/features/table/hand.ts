import {
  afterEveryRender,
  Component,
  computed,
  ElementRef,
  inject,
  input,
  output,
} from '@angular/core';
import type { Card } from '../../protocol/generated/protocol';
import { CardFace } from '../../ui/card';
import { cardLabel } from '../../ui/color-meta';

const MAX_ROTATION_STEP_DEGREES = 3.2;
const TOTAL_FAN_DEGREES = 36;
const MAX_REMEMBERED = 60;
/** Distance du pivot de l'éventail au centre d'une carte, en hauteurs de carte (`transform-origin: 50% 160%`). */
const FAN_PIVOT_RATIO = 1.1;

const fanOf = (styles: CSSStyleDeclaration): number =>
  parseFloat(styles.getPropertyValue('--fan')) || 1;

/** Même formule que `--step` dans hand.scss : le pas entre deux cartes pour une main de `count` cartes. */
function fanStep(
  count: number,
  width: number,
  cardWidth: number,
  gutter: number,
  maxStep: number,
): number {
  return Math.max(8, Math.min(maxStep, (width - 2 * gutter - cardWidth) / Math.max(count - 1, 1)));
}

/**
 * Ma main, en éventail : les cartes jouables sont surélevées et brillent, les autres sont légèrement atténuées mais
 * restent lisibles ; rien ne réagit hors de mon tour. Le chevauchement se resserre quand la main grossit.
 * Elle ne montre que les cartes arrivées : celles d'une pioche en cours (`incoming`) prennent leur place à leur
 * arrivée, et l'éventail se réorganise alors en douceur.
 */
@Component({
  selector: 'app-hand',
  imports: [CardFace],
  template: `
    <ul
      class="hand"
      data-anchor="hand"
      aria-label="Ta main"
      [style.--n]="cards().length"
      [style.--rot.deg]="rotationStep()"
    >
      @for (card of cards(); track card.id; let index = $index) {
        <li [attr.data-anchor]="'card:' + card.id" [style.--d]="index - (cards().length - 1) / 2">
          <button
            type="button"
            class="slot"
            [class.playable]="isPlayable(card)"
            [class.unplayable]="stateOf(card) === 'unplayable'"
            [class.waiting]="locked()"
            [disabled]="!isPlayable(card) || locked()"
            [attr.aria-label]="describe(card)"
            (click)="played.emit(card.id)"
          >
            <app-card [card]="card" [state]="stateOf(card)" />
          </button>
        </li>
      }
    </ul>
  `,
  styleUrl: './hand.scss',
})
export class Hand {
  readonly cards = input.required<readonly Card[]>();
  readonly playableIds = input.required<readonly number[]>();
  readonly myTurn = input.required<boolean>();
  /** Les cartes jouables sont montrées mais le serveur ne prend pas encore le coup : rien ne réagit. */
  readonly locked = input(false);
  /** Les cartes en vol vers ma main, dans l'ordre d'arrivée : absentes de `cards`, elles s'y ajoutent à la fin. */
  readonly incoming = input<readonly number[]>([]);
  readonly played = output<number>();

  private readonly host: HTMLElement = inject(ElementRef).nativeElement;
  private readonly remembered = new Map<number, DOMRect>();

  constructor() {
    // On retient où était chaque carte : une carte jouée a quitté la main avant que l'animation ne démarre
    afterEveryRender({
      read: () => {
        for (const slot of this.host.querySelectorAll<HTMLElement>('li[data-anchor]')) {
          this.remembered.set(
            Number(slot.dataset['anchor']?.slice(5)),
            slot.getBoundingClientRect(),
          );
        }
        while (this.remembered.size > MAX_REMEMBERED) {
          this.remembered.delete(this.remembered.keys().next().value as number);
        }
      },
    });
  }

  /**
   * La dernière position connue d'une carte de ma main, même si elle vient de la quitter ; pour une carte en vol, la
   * place qu'elle prendra à son arrivée.
   */
  lastRect(cardId: number): DOMRect | null {
    const live = this.host.querySelector('li[data-anchor="card:' + cardId + '"]');
    return live
      ? live.getBoundingClientRect()
      : (this.remembered.get(cardId) ?? this.arrivalRect(cardId));
  }

  /** Où sera la carte en vol `cardId` quand elle arrivera : à la fin d'une main qui compte les cartes arrivées avant elle. */
  private arrivalRect(cardId: number): DOMRect | null {
    const order = this.incoming().indexOf(cardId);
    const list = this.host.querySelector<HTMLElement>('ul.hand');
    const model = this.host.querySelector<HTMLElement>('li[data-anchor]');
    if (order < 0 || !list || !model) {
      return null;
    }
    const count = this.cards().length + order + 1;
    const styles = getComputedStyle(list);
    const hostBox = this.host.getBoundingClientRect();
    const { offsetWidth: width, offsetHeight: height } = model;
    const step = fanStep(
      count,
      this.host.clientWidth,
      width,
      parseFloat(styles.getPropertyValue('--gutter')),
      parseFloat(styles.getPropertyValue('--max-step')),
    );
    const fromCenter = count - 1 - (count - 1) / 2;
    const tilt =
      (fromCenter *
        Math.min(MAX_ROTATION_STEP_DEGREES, TOTAL_FAN_DEGREES / count) *
        fanOf(styles) *
        Math.PI) /
      180;
    const pivotDistance = height * FAN_PIVOT_RATIO;
    const listBox = list.getBoundingClientRect();
    const cx =
      hostBox.left + hostBox.width / 2 + fromCenter * step + pivotDistance * Math.sin(tilt);
    const cy =
      listBox.top +
      parseFloat(styles.paddingTop) +
      height / 2 +
      pivotDistance * (1 - Math.cos(tilt));
    return new DOMRect(cx - width / 2, cy - height / 2, width, height);
  }
  protected readonly rotationStep = computed(() =>
    Math.min(MAX_ROTATION_STEP_DEGREES, TOTAL_FAN_DEGREES / Math.max(1, this.cards().length)),
  );

  protected isPlayable(card: Card): boolean {
    return this.myTurn() && this.playableIds().includes(card.id);
  }

  protected stateOf(card: Card): 'neutral' | 'playable' | 'unplayable' {
    if (!this.myTurn()) {
      return 'neutral';
    }
    return this.playableIds().includes(card.id) ? 'playable' : 'unplayable';
  }

  protected describe(card: Card): string {
    return this.isPlayable(card) ? `${cardLabel(card)}, jouable` : cardLabel(card);
  }
}
