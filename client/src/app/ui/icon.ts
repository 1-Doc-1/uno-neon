import { Component, input } from '@angular/core';

export type IconName =
  | 'door'
  | 'crown'
  | 'copy'
  | 'check'
  | 'close'
  | 'arrow-cw'
  | 'arrow-ccw'
  | 'palette'
  | 'bot'
  | 'cards'
  | 'plus'
  | 'link';

/**
 * Icônes maison : des tracés SVG écrits ici, dessinés sur une grille de 24 × 24 (trait arrondi, `currentColor`).
 * Aucune dépendance, aucun `innerHTML`. L'icône est décorative : le texte ou l'`aria-label` du parent la décrit.
 */
@Component({
  selector: 'app-icon',
  template: `
    <svg
      viewBox="0 0 24 24"
      aria-hidden="true"
      fill="none"
      stroke="currentColor"
      stroke-width="2"
      stroke-linecap="round"
      stroke-linejoin="round"
    >
      @switch (name()) {
        @case ('door') {
          <path d="M14 3h5a1 1 0 0 1 1 1v16a1 1 0 0 1-1 1h-5" />
          <path d="M10 8l-4 4 4 4" />
          <path d="M6 12h10" />
        }
        @case ('crown') {
          <path d="M3 8l4.5 4L12 5l4.5 7L21 8l-2 11H5L3 8z" fill="currentColor" />
        }
        @case ('copy') {
          <rect x="9" y="9" width="11" height="11" rx="2" />
          <path d="M5 15V6a2 2 0 0 1 2-2h8" />
        }
        @case ('check') {
          <path d="M5 12.5l4.5 4.5L19 7.5" />
        }
        @case ('close') {
          <path d="M6 6l12 12M18 6L6 18" />
        }
        @case ('arrow-cw') {
          <path d="M20 12a8 8 0 1 1-2.5-5.8" />
          <path d="M20 4v5h-5" />
        }
        @case ('arrow-ccw') {
          <path d="M4 12a8 8 0 1 0 2.5-5.8" />
          <path d="M4 4v5h5" />
        }
        @case ('bot') {
          <rect x="5" y="8" width="14" height="11" rx="3" />
          <path d="M12 4v4" />
          <circle cx="12" cy="3.5" r="1" fill="currentColor" />
          <circle cx="9.5" cy="13" r="1.2" fill="currentColor" />
          <circle cx="14.5" cy="13" r="1.2" fill="currentColor" />
          <path d="M9.5 16.5h5" />
        }
        @case ('cards') {
          <rect x="4" y="6" width="10" height="14" rx="2" />
          <path d="M9 4h6a2 2 0 0 1 2 2v10" />
          <path d="M17 8l3 1v9" />
        }
        @case ('plus') {
          <path d="M12 5v14M5 12h14" />
        }
        @case ('link') {
          <path d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1" />
          <path d="M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1" />
        }
        @case ('palette') {
          <path
            d="M12 3a9 9 0 1 0 0 18c1.2 0 2-.8 2-1.8 0-.9-.6-1.4-.6-2.2 0-1 .8-1.6 1.8-1.6H17a4 4 0 0 0 4-4C21 6.7 17 3 12 3z"
          />
          <circle cx="7.5" cy="11" r="1" fill="currentColor" />
          <circle cx="11" cy="7.5" r="1" fill="currentColor" />
          <circle cx="15.5" cy="8.5" r="1" fill="currentColor" />
        }
      }
    </svg>
  `,
  styles: `
    :host {
      display: inline-block;
      width: 1.25em;
      height: 1.25em;
      vertical-align: -0.2em;
      flex: none;
    }
    svg {
      display: block;
      width: 100%;
      height: 100%;
    }
  `,
})
export class Icon {
  readonly name = input.required<IconName>();
}
