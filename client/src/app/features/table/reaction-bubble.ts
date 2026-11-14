import { Component, computed, input } from '@angular/core';
import type { Emote } from '../../protocol/generated/protocol';
import { emoteInfo } from '../../ui/emotes';

/**
 * La bulle d'une réaction rapide, posée sur le coin d'un siège. Elle se place par rapport au parent positionné le
 * plus proche (la pastille). Le pictogramme est décoratif : l'annonce « Zoé : Bien joué » passe par une zone `status`.
 */
@Component({
  selector: 'app-reaction-bubble',
  template: `
    @if (info(); as shown) {
      <span
        class="bubble"
        animate.enter="bubble-in"
        animate.leave="bubble-out"
        aria-hidden="true"
        >{{ shown.glyph }}</span
      >
    }
    <span class="sr-only" role="status">{{ announcement() }}</span>
  `,
  styles: `
    :host {
      display: contents;
    }
    .bubble {
      position: absolute;
      top: calc(var(--space-3) * -1.6);
      right: calc(var(--space-2) * -1);
      z-index: 3;
      display: grid;
      place-items: center;
      min-width: 2.1em;
      height: 2.1em;
      padding: 0 0.3em;
      border-radius: var(--radius-pill);
      background: var(--panel-solid);
      box-shadow: var(--elevation);
      font-size: clamp(1.1rem, 2.6vh, 1.6rem);
      line-height: 1;
      pointer-events: none;
    }
    .bubble-in {
      animation: bubble-in var(--dur-base) var(--ease-out) both;
    }
    .bubble-out {
      animation: bubble-out var(--dur-base) ease-in both;
    }
    @keyframes bubble-in {
      from {
        opacity: 0;
        transform: translateY(8px) scale(0.5);
      }
    }
    @keyframes bubble-out {
      to {
        opacity: 0;
        transform: translateY(-6px) scale(0.8);
      }
    }
    @media (prefers-reduced-motion: reduce) {
      .bubble-in,
      .bubble-out {
        animation-name: none;
      }
    }
  `,
})
export class ReactionBubble {
  /** La réaction à montrer, ou `null` quand il n'y en a pas. */
  readonly emote = input<Emote | null>(null);
  /** Le pseudo de l'émetteur, pour l'annonce aux lecteurs d'écran. */
  readonly nickname = input.required<string>();

  protected readonly info = computed(() => {
    const emote = this.emote();
    return emote === null ? null : emoteInfo(emote);
  });

  protected readonly announcement = computed(() => {
    const shown = this.info();
    return shown === null ? '' : `${this.nickname()} : ${shown.label}`;
  });
}
