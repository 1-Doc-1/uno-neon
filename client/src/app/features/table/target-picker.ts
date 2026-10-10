import { afterNextRender, Component, ElementRef, input, output, viewChild } from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { Avatar } from '../../ui/avatar';
import { Button } from '../../ui/button';

/**
 * Le choix de la cible d'un Joker +5 : une fenêtre qui liste les adversaires (initiale, pseudo, nombre de cartes).
 * Elle n'est pas modale : les sièges de la table restent cliquables, c'est l'autre façon de viser. Au clavier : Tab et
 * Entrée, ou les chiffres 1 à 9 ; Échap annule.
 */
@Component({
  selector: 'app-target-picker',
  imports: [Avatar, Button],
  host: { '(document:keydown)': 'onKey($event)' },
  template: `
    <dialog #dialog class="panel" aria-label="Choisir la cible du Joker +5">
      <h2>Qui vises-tu ?</h2>
      <p class="hint">
        Il pioche 5 cartes, sauf s’il répond avec un Joker +5. Clique aussi sur un siège.
      </p>
      <ul>
        @for (seat of targets(); track seat.playerId; let index = $index) {
          <li>
            <button type="button" class="target" (click)="picked.emit(seat.playerId)">
              <app-avatar [playerId]="seat.playerId" [nickname]="seat.nickname" />
              <span class="name">{{ seat.nickname }}</span>
              <span class="cards"
                >{{ seat.cardCount }} carte{{ seat.cardCount > 1 ? 's' : '' }}</span
              >
              <kbd aria-hidden="true">{{ index + 1 }}</kbd>
            </button>
          </li>
        }
      </ul>
      <button appButton kind="ghost" (click)="cancelled.emit()">Annuler</button>
    </dialog>
  `,
  styles: `
    dialog {
      position: fixed;
      inset: auto;
      top: 50%;
      left: 50%;
      translate: -50% -50%;
      z-index: var(--z-modal);
      width: min(92vw, 380px);
      max-height: 80dvh;
      margin: 0;
      padding: var(--space-4);
      border: 1px solid var(--gold-edge);
      color: var(--text);
      overflow: auto;
      box-shadow:
        var(--elevation),
        0 0 18px color-mix(in oklab, var(--gold-edge) 45%, transparent);
    }
    dialog:not([open]) {
      display: none;
    }
    dialog[open] {
      display: grid;
      gap: var(--space-3);
      justify-items: center;
    }
    h2 {
      font-size: var(--fs-lg);
    }
    .hint {
      color: var(--text-dim);
      font-size: var(--fs-xs);
      text-align: center;
    }
    ul {
      display: grid;
      gap: var(--space-2);
      width: 100%;
      margin: 0;
      padding: 0;
      list-style: none;
    }
    .target {
      display: grid;
      grid-template-columns: auto 1fr auto auto;
      align-items: center;
      gap: var(--space-3);
      width: 100%;
      min-height: 52px;
      padding: var(--space-2) var(--space-3);
      border: 2px solid var(--gold-deep);
      border-radius: var(--radius-md);
      background: var(--field);
      color: var(--text);
      font-family: var(--font-display);
      font-weight: 700;
      text-align: left;
      cursor: pointer;
      transition: transform var(--dur-fast) var(--ease-out);
    }
    .target:hover,
    .target:focus-visible {
      border-color: var(--gold-edge);
      transform: translateY(-2px);
    }
    .name {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .cards {
      color: var(--text-dim);
      font-size: var(--fs-xs);
      font-weight: 400;
    }
    kbd {
      color: var(--text-dim);
      font-family: var(--font-mono);
      font-size: var(--fs-xs);
    }
  `,
})
export class TargetPicker {
  /** Les adversaires qu'on peut viser, dans l'ordre de la table. */
  readonly targets = input.required<readonly SeatView[]>();
  readonly picked = output<string>();
  readonly cancelled = output<void>();

  private readonly dialog = viewChild.required<ElementRef<HTMLDialogElement>>('dialog');

  constructor() {
    afterNextRender(() => {
      const element = this.dialog().nativeElement;
      element.show();
      element.querySelector<HTMLElement>('.target')?.focus();
    });
  }

  protected onKey(event: KeyboardEvent): void {
    if (event.key === 'Escape') {
      this.cancelled.emit();
      return;
    }
    const seat = this.targets()[Number(event.key) - 1];
    if (seat) {
      this.picked.emit(seat.playerId);
    }
  }
}
