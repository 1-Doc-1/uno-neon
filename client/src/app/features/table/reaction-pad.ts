import { Component, ElementRef, inject, input, output, signal } from '@angular/core';
import type { Emote } from '../../protocol/generated/protocol';
import { ClickSound } from '../../ui/click-sound';
import { EMOTES } from '../../ui/emotes';

/**
 * Le sélecteur de réactions rapides : un bouton qui déplie les six pictogrammes prédéfinis (aucun texte libre). Un
 * choix envoie la réaction et replie le sélecteur ; il est grisé pendant le délai que le serveur impose entre deux envois.
 */
@Component({
  selector: 'app-reaction-pad',
  imports: [ClickSound],
  template: `
    <div class="pad">
      <button
        type="button"
        class="toggle"
        [attr.aria-expanded]="open()"
        aria-controls="reaction-choices"
        (click)="open.update((isOpen) => !isOpen)"
      >
        <span aria-hidden="true">😀</span> Réagir
      </button>
      @if (open()) {
        <div id="reaction-choices" class="choices" role="group" aria-label="Réactions rapides">
          @for (emote of emotes; track emote.id) {
            <button
              type="button"
              class="choice"
              [attr.aria-label]="emote.label"
              [title]="emote.label"
              [disabled]="cooling()"
              (click)="pick(emote.id)"
            >
              <span aria-hidden="true">{{ emote.glyph }}</span>
            </button>
          }
        </div>
      }
    </div>
  `,
  styles: `
    :host {
      display: block;
    }
    .pad {
      position: relative;
    }
    .toggle {
      display: inline-flex;
      align-items: center;
      gap: var(--space-2);
      min-height: 44px;
      padding: var(--space-2) var(--space-4);
      border: 1px solid var(--panel-solid);
      border-radius: var(--radius-pill);
      background: var(--panel-solid);
      color: var(--text);
      font-family: var(--font-display);
      font-weight: 700;
      cursor: pointer;
    }
    .choices {
      position: absolute;
      right: 0;
      bottom: calc(100% + var(--space-2));
      display: grid;
      grid-template-columns: repeat(3, 44px);
      gap: var(--space-2);
      padding: var(--space-3);
      border-radius: var(--radius-lg);
      background: var(--panel-solid);
      box-shadow: var(--elevation);
    }
    .choice {
      display: grid;
      place-items: center;
      width: 44px;
      height: 44px;
      padding: 0;
      border: 1px solid transparent;
      border-radius: var(--radius-pill);
      background: transparent;
      font-size: 1.5rem;
      line-height: 1;
      cursor: pointer;
    }
    .choice:hover:not(:disabled) {
      border-color: var(--text-dim);
    }
    .choice:disabled {
      cursor: not-allowed;
      filter: grayscale(1) brightness(0.7);
    }
    .toggle:focus-visible,
    .choice:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: 2px;
    }
  `,
  host: { '(keydown.escape)': 'open.set(false)', '(focusout)': 'onFocusOut($event)' },
})
export class ReactionPad {
  /** Vrai pendant le délai entre deux envois : les pictogrammes sont grisés. */
  readonly cooling = input(false);
  readonly picked = output<Emote>();

  protected readonly emotes = EMOTES;
  protected readonly open = signal(false);
  private readonly host = inject<ElementRef<HTMLElement>>(ElementRef);

  /** Le focus part ailleurs (le chat, une carte) : le sélecteur se replie, pour ne rien recouvrir. */
  protected onFocusOut(event: FocusEvent): void {
    if (!this.host.nativeElement.contains(event.relatedTarget as Node | null)) {
      this.open.set(false);
    }
  }

  protected pick(emote: Emote): void {
    this.picked.emit(emote);
    this.open.set(false);
  }
}
