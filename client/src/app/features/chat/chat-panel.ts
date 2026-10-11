import {
  afterRenderEffect,
  Component,
  computed,
  ElementRef,
  input,
  output,
  signal,
  viewChild,
} from '@angular/core';
import { Avatar } from '../../ui/avatar';
import { Button } from '../../ui/button';
import { ClickSound } from '../../ui/click-sound';

/** Une ligne de chat prête à afficher. */
export interface ChatDisplayLine {
  readonly id: number;
  readonly nickname: string;
  readonly avatarId: number;
  readonly text: string;
  readonly mine: boolean;
}

export const CHAT_MAX_LENGTH = 200;

/**
 * Le panneau de chat, replié par défaut : un bouton avec le nombre de messages non lus, puis la liste et le champ de
 * saisie. Le texte est toujours inséré comme du texte (jamais comme du HTML, ADR 0037).
 */
@Component({
  selector: 'app-chat-panel',
  imports: [ClickSound, Avatar, Button],
  template: `
    <section class="chat" [class.open]="open()" aria-label="Chat du salon">
      <button
        type="button"
        class="toggle"
        [attr.aria-expanded]="open()"
        aria-controls="chat-body"
        (click)="toggled.emit(!open())"
      >
        <span class="label">Chat</span>
        @if (unread() > 0) {
          <span class="unread" [attr.aria-label]="unread() + ' messages non lus'">{{
            unread()
          }}</span>
        }
      </button>
      @if (open()) {
        <div id="chat-body" class="body">
          <ul class="lines" #list aria-live="polite" aria-label="Messages">
            @for (line of lines(); track line.id) {
              <li [class.mine]="line.mine">
                <app-avatar [avatarId]="line.avatarId" class="who" />
                <p>
                  <strong>{{ line.nickname }}</strong> {{ line.text }}
                </p>
              </li>
            } @empty {
              <li class="empty">Aucun message pour l’instant.</li>
            }
          </ul>
          <form (submit)="send($event)">
            <label class="sr-only" for="chat-input">Ton message</label>
            <input
              id="chat-input"
              type="text"
              autocomplete="off"
              [maxLength]="maxLength"
              placeholder="Écris un message…"
              [value]="draft()"
              (input)="onInput($event)"
            />
            <button appButton kind="primary" [disabled]="!canSend()">Envoyer</button>
          </form>
        </div>
      }
    </section>
  `,
  styles: `
    :host {
      display: block;
    }
    .chat {
      display: grid;
      justify-items: end;
      gap: var(--space-2);
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
    .toggle:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: 2px;
    }
    .unread {
      display: inline-grid;
      place-items: center;
      min-width: 1.5em;
      height: 1.5em;
      padding: 0 0.4em;
      border-radius: var(--radius-pill);
      background: var(--game-red);
      color: var(--text-on-neon);
      font-size: var(--fs-xs);
    }
    .body {
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: var(--space-2);
      width: min(92vw, 340px);
      padding: var(--space-3);
      border-radius: var(--radius-lg);
      background: var(--panel-solid);
      box-shadow: var(--elevation);
    }
    .lines {
      display: grid;
      align-content: start;
      gap: var(--space-2);
      height: min(40dvh, 260px);
      margin: 0;
      padding: 0;
      overflow-y: auto;
      list-style: none;
    }
    li {
      display: flex;
      gap: var(--space-2);
      align-items: flex-start;
      font-size: var(--fs-sm);
    }
    li p {
      margin: 0;
      overflow-wrap: anywhere;
    }
    li.mine strong {
      color: var(--game-green);
    }
    .empty {
      color: var(--text-dim);
    }
    .who {
      font-size: 0.8rem;
    }
    form {
      display: flex;
      gap: var(--space-2);
    }
    input {
      flex: 1;
      min-width: 0;
      padding: var(--space-2) var(--space-3);
      border: 1px solid var(--text-dim);
      border-radius: var(--radius-md);
      background: var(--field);
      color: var(--text);
      font: inherit;
    }
    input:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: 1px;
    }
  `,
})
export class ChatPanel {
  readonly lines = input.required<readonly ChatDisplayLine[]>();
  readonly open = input(false);
  readonly unread = input(0);
  readonly toggled = output<boolean>();
  readonly sent = output<string>();

  protected readonly maxLength = CHAT_MAX_LENGTH;
  protected readonly draft = signal('');
  protected readonly canSend = computed(() => this.draft().trim().length > 0);
  private readonly list = viewChild<ElementRef<HTMLElement>>('list');

  constructor() {
    // Les messages récents sont en bas : la liste suit
    afterRenderEffect(() => {
      this.lines();
      const element = this.list()?.nativeElement;
      if (element) {
        element.scrollTop = element.scrollHeight;
      }
    });
  }

  protected onInput(event: Event): void {
    this.draft.set((event.target as HTMLInputElement).value);
  }

  protected send(event: Event): void {
    event.preventDefault();
    const text = this.draft().trim();
    if (text.length > 0) {
      this.sent.emit(text);
      this.draft.set('');
    }
  }
}
