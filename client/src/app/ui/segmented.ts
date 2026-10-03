import { Component, input, output } from '@angular/core';

export interface SegmentOption<T> {
  readonly value: T;
  readonly label: string;
}

/** Choix exclusif entre quelques valeurs (SPEC §11.5). */
@Component({
  selector: 'app-segmented',
  template: `
    <div role="radiogroup" [attr.aria-label]="label()">
      @for (option of options(); track option.value) {
        <button
          type="button"
          role="radio"
          [attr.aria-checked]="option.value === value()"
          [disabled]="disabled()"
          (click)="selected.emit(option.value)"
        >
          {{ option.label }}
        </button>
      }
    </div>
  `,
  styles: `
    div {
      display: inline-flex;
      flex-wrap: wrap;
      gap: var(--space-1);
      padding: var(--space-1);
      border-radius: var(--radius-lg);
      background: var(--field);
    }
    button {
      min-height: 36px;
      min-width: 36px;
      padding: 0 var(--space-3);
      border: 0;
      border-radius: var(--radius-pill);
      background: transparent;
      color: var(--text-dim);
      cursor: pointer;
    }
    button[aria-checked='true'] {
      background: var(--text);
      color: var(--text-on-neon);
      font-weight: 700;
    }
    button:disabled {
      cursor: default;
    }
    button:disabled:not([aria-checked='true']) {
      opacity: 0.55;
    }
  `,
})
export class Segmented<T extends string | number> {
  readonly options = input.required<readonly SegmentOption<T>[]>();
  readonly value = input.required<T>();
  readonly label = input.required<string>();
  readonly disabled = input(false);
  readonly selected = output<T>();
}
