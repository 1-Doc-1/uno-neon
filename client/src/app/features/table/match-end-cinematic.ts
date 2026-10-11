import {
  afterNextRender,
  Component,
  computed,
  DestroyRef,
  DOCUMENT,
  inject,
  input,
  output,
  signal,
} from '@angular/core';
import type { SeatView } from '../../protocol/generated/protocol';
import { AudioService } from '../../audio/audio.service';
import { Avatar } from '../../ui/avatar';
import { Button } from '../../ui/button';
import { CardBack } from '../../ui/card-back';
import { ClickSound } from '../../ui/click-sound';

/** Combien de temps dure la cinématique, jusqu'à l'écran des choix ; plus courte en mouvement réduit. */
export const CINEMATIC_MS = 8000;
export const CINEMATIC_REDUCED_MS = 3500;
/** Les scores défilent de zéro à leur valeur pendant ce temps, après l'arrivée du podium. */
const COUNT_DELAY_MS = 900;
const COUNT_MS = 1600;
const CONFETTI = Array.from({ length: 44 }, (_, index) => index);
const CARDS = Array.from({ length: 12 }, (_, index) => index);

/** Le défilement d'un score : de 0 à `score`, selon l'avancement `progress` (0 à 1), toujours entier. */
export function scoreAt(score: number, progress: number): number {
  return Math.round(score * Math.min(1, Math.max(0, progress)));
}

/**
 * La cinématique de fin de partie : le podium (1er, 2e, 3e avec leurs avatars), les scores qui défilent, une pluie de
 * cartes et de confettis aux couleurs du jeu, et le son de la victoire ou de la défaite. Passable d'un clic ; en
 * mouvement réduit, un simple fondu. Elle s'efface seule, puis vient l'écran Revanche / Retour au salon (ADR 0038).
 */
@Component({
  selector: 'app-match-end-cinematic',
  imports: [ClickSound, Avatar, Button, CardBack],
  template: `
    <div
      class="cinematic"
      role="dialog"
      aria-label="Podium de la partie"
      tabindex="-1"
      #root
      (click)="finish()"
      (keydown.enter)="finish()"
      (keydown.space)="finish()"
      (keydown.escape)="finish()"
    >
      <div class="rain" aria-hidden="true">
        @for (index of cards; track index) {
          <app-card-back class="drop" [style.--k]="index" [style.left.%]="column(index, 5)" />
        }
        @for (index of confetti; track index) {
          <i class="confetti" [style.--k]="index" [style.left.%]="column(index, 11)"></i>
        }
      </div>
      <h2>{{ winner()?.nickname }} remporte la partie !</h2>
      <p class="mood">{{ iWon() ? 'Bravo, tu as gagné !' : 'Belle partie !' }}</p>
      <ol class="podium">
        @for (place of podium(); track place.player.playerId) {
          <li [class]="'place rank-' + place.rank">
            <app-avatar [avatarId]="place.player.avatarId" class="avatar" />
            <span class="name">{{ place.player.nickname }}</span>
            <span class="score">{{ shown(place.player.score) }} pts</span>
            <span class="step" aria-hidden="true">{{ place.rank }}</span>
          </li>
        }
      </ol>
      @if (others().length > 0) {
        <ul class="others">
          @for (player of others(); track player.playerId) {
            <li>
              <span>{{ player.nickname }}</span> <span>{{ shown(player.score) }} pts</span>
            </li>
          }
        </ul>
      }
      <button appButton kind="ghost" class="skip" (click)="finish(); $event.stopPropagation()">
        Passer
      </button>
    </div>
  `,
  styles: `
    :host {
      position: fixed;
      inset: 0;
      z-index: var(--z-modal);
    }
    .cinematic {
      position: absolute;
      inset: 0;
      display: grid;
      align-content: center;
      justify-items: center;
      gap: var(--space-4);
      padding: var(--space-5);
      overflow: hidden;
      background: radial-gradient(ellipse at 50% 40%, rgb(34 44 80 / 0.96), rgb(8 12 28 / 0.98));
      color: var(--text);
      text-align: center;
      cursor: pointer;
      animation: fade-in 500ms ease-out both;
    }
    .cinematic:focus-visible {
      outline: 3px solid var(--focus-ring);
      outline-offset: -6px;
    }
    h2 {
      z-index: 1;
      font-size: clamp(1.6rem, 5vw, 2.6rem);
      animation: title-in 700ms var(--ease-out) 150ms both;
    }
    .mood {
      z-index: 1;
      color: var(--text-dim);
      animation: fade-in 600ms ease-out 500ms both;
    }
    .podium {
      z-index: 1;
      display: flex;
      align-items: flex-end;
      justify-content: center;
      gap: var(--space-3);
      width: min(100%, 560px);
      margin: 0;
      padding: 0;
      list-style: none;
    }
    .place {
      display: grid;
      justify-items: center;
      align-content: end;
      gap: var(--space-2);
      flex: 1 1 0;
      min-width: 0;
      animation: rise 700ms var(--ease-out) both;
    }
    .rank-1 {
      order: 2;
      --h: 120px;
      animation-delay: 650ms;
      font-size: 1.2em;
    }
    .rank-2 {
      order: 1;
      --h: 84px;
      animation-delay: 400ms;
    }
    .rank-3 {
      order: 3;
      --h: 58px;
      animation-delay: 150ms;
    }
    .avatar {
      font-size: clamp(1.6rem, 6vw, 2.6rem);
    }
    .rank-1 .avatar {
      box-shadow:
        0 0 0 3px var(--uno),
        0 0 26px var(--uno);
    }
    .name {
      max-width: 100%;
      overflow: hidden;
      font-family: var(--font-display);
      font-weight: 700;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .score {
      font-family: var(--font-display);
      font-variant-numeric: tabular-nums;
    }
    .step {
      display: grid;
      place-items: center;
      width: 100%;
      height: var(--h);
      border-radius: var(--radius-md) var(--radius-md) 0 0;
      background: var(--panel-solid);
      font-family: var(--font-display);
      font-size: 2rem;
      font-weight: 700;
    }
    .rank-1 .step {
      background: color-mix(in oklab, var(--uno) 55%, var(--panel-solid));
      color: var(--text-on-neon);
    }
    .others {
      z-index: 1;
      display: grid;
      gap: var(--space-1);
      margin: 0;
      padding: 0;
      color: var(--text-dim);
      font-size: var(--fs-sm);
      list-style: none;
    }
    .others li {
      display: flex;
      gap: var(--space-4);
      justify-content: space-between;
    }
    .skip {
      z-index: 2;
      position: absolute;
      top: var(--space-3);
      right: var(--space-3);
    }
    // La pluie : des cartes de dos et des confettis aux couleurs du jeu, répartis d'après leur rang (rien d'aléatoire)
    .rain {
      position: absolute;
      inset: 0;
      pointer-events: none;
    }
    .drop,
    .confetti {
      position: absolute;
      top: -12vh;
      animation: fall calc(3200ms + var(--k) * 90ms) linear calc(var(--k) * 173ms) infinite;
    }
    .drop {
      --card-w: clamp(34px, 6vw, 56px);
      opacity: 0.9;
    }
    .confetti {
      width: 10px;
      height: 16px;
      border-radius: 2px;
      background: var(--game-red);
    }
    .confetti:nth-child(4n + 2) {
      background: var(--game-yellow);
    }
    .confetti:nth-child(4n + 3) {
      background: var(--game-green);
    }
    .confetti:nth-child(4n + 4) {
      background: var(--game-blue);
    }
    @keyframes fall {
      from {
        transform: translateY(0) rotate(0deg);
      }
      to {
        transform: translateY(125vh) rotate(540deg);
      }
    }
    @keyframes rise {
      from {
        opacity: 0;
        transform: translateY(70px) scale(0.85);
      }
    }
    @keyframes title-in {
      from {
        opacity: 0;
        transform: scale(0.6);
      }
    }
    @keyframes fade-in {
      from {
        opacity: 0;
      }
    }
    @media (prefers-reduced-motion: reduce) {
      .rain {
        display: none;
      }
      .place,
      h2,
      .mood {
        animation: fade-in 400ms ease-out both;
      }
    }
  `,
})
export class MatchEndCinematic {
  readonly players = input.required<readonly SeatView[]>();
  readonly winnerId = input.required<string | null>();
  /** Mon identifiant : la cinématique joue le son de victoire ou de défaite. */
  readonly meId = input.required<string>();
  readonly finished = output<void>();

  protected readonly cards = CARDS;
  protected readonly confetti = CONFETTI;
  private readonly progress = signal(0);
  private readonly document = inject(DOCUMENT);
  private readonly audio = inject(AudioService);
  private readonly destroyRef = inject(DestroyRef);
  private done = false;

  protected readonly ranking = computed(() =>
    [...this.players()].sort((a, b) => b.score - a.score),
  );
  /** Les trois premiers, avec leur rang (1 au centre, 2 à sa gauche, 3 à sa droite). */
  protected readonly podium = computed(() =>
    this.ranking()
      .slice(0, 3)
      .map((player, index) => ({ player, rank: index + 1 })),
  );
  protected readonly others = computed(() => this.ranking().slice(3));
  protected readonly winner = computed(() =>
    this.players().find((p) => p.playerId === this.winnerId()),
  );
  protected readonly iWon = computed(() => this.winnerId() === this.meId());

  constructor() {
    afterNextRender(() => {
      const win = this.document.defaultView;
      const reduced = win?.matchMedia?.('(prefers-reduced-motion: reduce)').matches === true;
      this.audio.play(this.iWon() ? 'win' : 'lose');
      if (win === null || reduced) {
        this.progress.set(1);
      } else {
        this.countUp(win);
      }
      const timer = win?.setTimeout(
        () => this.finish(),
        reduced ? CINEMATIC_REDUCED_MS : CINEMATIC_MS,
      );
      this.destroyRef.onDestroy(() => win?.clearTimeout(timer));
      this.document.querySelector<HTMLElement>('app-match-end-cinematic .cinematic')?.focus();
    });
  }

  /** La colonne (en %) d'une carte ou d'un confetti : répartie d'après son rang, sans rien d'aléatoire. */
  protected column(index: number, seed: number): number {
    return (index * 37 + seed * 13) % 96;
  }

  protected shown(score: number): number {
    return scoreAt(score, this.progress());
  }

  protected finish(): void {
    if (!this.done) {
      this.done = true;
      this.finished.emit();
    }
  }

  private countUp(win: Window): void {
    const start = win.performance.now() + COUNT_DELAY_MS;
    let frame = 0;
    const step = (now: number): void => {
      this.progress.set((now - start) / COUNT_MS);
      if (now - start < COUNT_MS) {
        frame = win.requestAnimationFrame(step);
      } else {
        this.progress.set(1);
      }
    };
    frame = win.requestAnimationFrame(step);
    this.destroyRef.onDestroy(() => win.cancelAnimationFrame(frame));
  }
}
