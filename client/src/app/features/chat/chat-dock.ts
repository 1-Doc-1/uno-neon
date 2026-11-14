import { Component, computed, inject, signal } from '@angular/core';
import { GameStore } from '../../state/game-store';
import { ChatDisplayLine, ChatPanel } from './chat-panel';

/** Le chat du salon, relié au store : il vit au salon comme à la table. */
@Component({
  selector: 'app-chat-dock',
  imports: [ChatPanel],
  template: `
    <ng-content />
    <app-chat-panel
      [lines]="lines()"
      [open]="open()"
      [unread]="store.chatUnread()"
      (toggled)="toggle($event)"
      (sent)="store.sendChat($event)"
    />
  `,
  host: { class: 'dock' },
  styles: `
    :host {
      position: fixed;
      right: var(--space-3);
      bottom: var(--space-3);
      z-index: calc(var(--z-table) + 5);
      // Le contenu projeté (les réactions à la table) se range à gauche du chat, ou au-dessous si la place manque
      display: flex;
      flex-flow: row wrap-reverse;
      justify-content: flex-end;
      align-items: flex-end;
      gap: var(--space-2);
      max-width: calc(100vw - 2 * var(--space-3));
    }
  `,
})
export class ChatDock {
  protected readonly store = inject(GameStore);
  protected readonly open = signal(false);

  protected readonly lines = computed<readonly ChatDisplayLine[]>(() => {
    const players = this.store.room()?.players ?? [];
    return this.store.chat().map((line) => {
      const author = players.find((p) => p.playerId === line.playerId);
      return {
        id: line.id,
        nickname: author?.nickname ?? '?',
        avatarId: author?.avatarId ?? 0,
        text: line.text,
        mine: line.playerId === this.store.playerId(),
      };
    });
  });

  protected toggle(open: boolean): void {
    this.open.set(open);
    this.store.setChatOpen(open);
  }
}
