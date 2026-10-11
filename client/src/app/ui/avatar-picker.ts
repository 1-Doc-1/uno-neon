import { Component, input, output } from '@angular/core';
import { ClickSound } from './click-sound';
import { Avatar, AVATAR_IDS, avatarName } from './avatar';

/** Le choix de l'avatar : les 12 personnages, un groupe de boutons radio (flèches du clavier comprises). */
@Component({
  selector: 'app-avatar-picker',
  imports: [ClickSound, Avatar],
  template: `
    <div class="grid" role="radiogroup" aria-label="Ton avatar">
      @for (id of ids; track id) {
        <button
          type="button"
          role="radio"
          class="avatar"
          [attr.aria-checked]="id === selected()"
          [attr.aria-label]="name(id)"
          [attr.title]="name(id)"
          [tabindex]="id === selected() ? 0 : -1"
          (click)="picked.emit(id)"
          (keydown)="onKey($event)"
        >
          <app-avatar [avatarId]="id" />
        </button>
      }
    </div>
  `,
  styles: `
    .grid {
      display: grid;
      grid-template-columns: repeat(6, 1fr);
      gap: var(--space-2);
    }
    .avatar {
      display: grid;
      place-items: center;
      padding: var(--space-1);
      border: 2px solid transparent;
      border-radius: var(--radius-md);
      background: transparent;
      font-size: 1rem;
      cursor: pointer;
      transition:
        transform var(--dur-fast),
        border-color var(--dur-fast);
    }
    .avatar:hover {
      transform: scale(1.06);
    }
    .avatar[aria-checked='true'] {
      border-color: var(--text);
      background: var(--field);
    }
    .avatar:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: 2px;
    }
    app-avatar {
      width: 100%;
      height: auto;
      aspect-ratio: 1;
    }
    @media (max-width: 420px) {
      .grid {
        gap: var(--space-1);
      }
    }
  `,
})
export class AvatarPicker {
  readonly selected = input.required<number>();
  readonly picked = output<number>();

  protected readonly ids = AVATAR_IDS;
  protected readonly name = avatarName;

  /** Les flèches changent d'avatar comme dans tout groupe radio ; Origine et Fin vont aux extrémités. */
  protected onKey(event: KeyboardEvent): void {
    const step = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[event.key];
    let next: number | null = null;
    if (step !== undefined) {
      next = (this.selected() + step + this.ids.length) % this.ids.length;
    } else if (event.key === 'Home') {
      next = 0;
    } else if (event.key === 'End') {
      next = this.ids.length - 1;
    }
    if (next !== null) {
      event.preventDefault();
      this.picked.emit(next);
      const buttons = (event.currentTarget as HTMLElement)
        .closest('[role="radiogroup"]')
        ?.querySelectorAll<HTMLElement>('button');
      queueMicrotask(() => buttons?.[next]?.focus());
    }
  }
}
