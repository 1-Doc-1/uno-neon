import { afterNextRender, Component, ElementRef, input, output, viewChild } from '@angular/core';

/**
 * Fenêtre modale basée sur `<dialog>` : le navigateur piège le focus et gère Échap.
 * Une modale non fermable (`dismissible = false`) ne se ferme que quand son parent la retire du DOM.
 */
@Component({
  selector: 'app-modal',
  template: `
    <dialog #dialog class="glass strong" [attr.aria-label]="label()" (cancel)="onCancel($event)">
      <ng-content />
    </dialog>
  `,
  styles: `
    dialog {
      width: min(92vw, 520px);
      max-height: 90dvh;
      padding: var(--space-5);
      border: 1px solid var(--surface-border);
      color: var(--text-primary);
      overflow: auto;
    }
    dialog::backdrop {
      background: color-mix(in oklab, var(--bg-sky-top) 60%, transparent);
      backdrop-filter: blur(4px);
    }
    dialog[open] {
      animation: modal-in var(--dur-base) var(--ease-out);
    }
    @keyframes modal-in {
      from {
        opacity: 0;
        transform: translateY(12px) scale(0.98);
      }
    }
  `,
})
export class Modal {
  readonly label = input.required<string>();
  readonly dismissible = input(false);
  readonly dismissed = output<void>();

  private readonly dialog = viewChild.required<ElementRef<HTMLDialogElement>>('dialog');

  constructor() {
    afterNextRender(() => this.dialog().nativeElement.showModal());
  }

  protected onCancel(event: Event): void {
    event.preventDefault();
    if (this.dismissible()) {
      this.dismissed.emit();
    }
  }
}
