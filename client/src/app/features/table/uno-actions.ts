import { Component, input, output } from '@angular/core';

/** Un bouton « Contre-UNO ! » : un par joueur dont la fenêtre est ouverte. */
export interface CatchButton {
  readonly playerId: string;
  readonly nickname: string;
  /** Pendant la grâce, seul le fautif peut encore annoncer : le bouton est grisé avec son compte à rebours. */
  readonly inGrace: boolean;
  readonly secondsLeft: number;
}

/** Le bouton UNO et, empilés au même endroit, un « Contre-UNO ! » par cible (SPEC §12.3). */
@Component({
  selector: 'app-uno-actions',
  template: `
    <div class="actions" role="group" aria-label="Annonces UNO">
      @for (button of catchButtons(); track button.playerId) {
        <button
          type="button"
          class="catch"
          [class.waiting]="button.inGrace"
          [attr.aria-disabled]="button.inGrace"
          (click)="tryCatch(button)"
        >
          Contre-UNO ! {{ button.nickname }}
          <span class="count">{{ button.inGrace ? 'dans ' : '' }}{{ button.secondsLeft }} s</span>
        </button>
      }
      @if (canCallUno()) {
        <button
          type="button"
          class="uno"
          [class.must]="mustDeclareUno()"
          (click)="unoCalled.emit()"
        >
          UNO !
        </button>
      }
      @if (mustDeclareUno()) {
        <p class="hint" role="status">Annonce UNO pour poser ta dernière carte.</p>
      }
    </div>
  `,
  styleUrl: './uno-actions.scss',
})
export class UnoActions {
  readonly catchButtons = input.required<readonly CatchButton[]>();
  readonly canCallUno = input.required<boolean>();
  readonly mustDeclareUno = input.required<boolean>();
  readonly unoCalled = output<void>();
  readonly caught = output<string>();

  /** Un bouton grisé (pendant la grâce) est atténué mais reste focalisable ; le clic n'y fait rien. */
  protected tryCatch(button: CatchButton): void {
    if (!button.inGrace) {
      this.caught.emit(button.playerId);
    }
  }
}
