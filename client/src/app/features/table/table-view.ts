import {
  Component,
  computed,
  DestroyRef,
  effect,
  ElementRef,
  inject,
  input,
  output,
  signal,
  untracked,
  viewChild,
} from '@angular/core';
import { AudioService } from '../../audio/audio.service';
import type { Card, Color, PlayerView } from '../../protocol/generated/protocol';
import type { EventBatch } from '../../state/game-store';
import { ChallengeDialog } from './challenge-dialog';
import { ColorPicker } from './color-picker';
import { AnimationDirector } from './fx/animation-director';
import { EffectsLayer } from './fx/effects-layer';
import { Hand } from './hand';
import { MatchOverDialog } from './match-over-dialog';
import { MyBadge } from './my-badge';
import { MAX_FAN_BACKS } from './opponent-fan';
import { OpponentSeat } from './opponent-seat';
import { Piles } from './piles';
import { PlusFivePrompt } from './plus-five-prompt';
import { SoundControl } from '../../ui/sound-control';
import { RoundOverDialog } from './round-over-dialog';
import { opponentsInViewOrder, seatLayout } from './seat-layout';
import { TableCenter } from './table-center';
import { TargetPicker } from './target-picker';
import { Size, tableGeometry } from './table-geometry';
import { CatchButton, UnoActions } from './uno-actions';

const CLOCK_TICK_MS = 250;
const NARROW_QUERY = '(max-width: 639px)';
/** En portrait étroit : au-delà, les adversaires passent en bande défilante. */
const MAX_ARC_OPPONENTS = 4;
const NARROW_MAX_BACKS = 5;
const JOURNAL_LINES = 2;
/** Les dernières secondes de mon tour, pendant lesquelles la musique monte en tension. */
const TENSION_SECONDS = 5;

export interface CardPlay {
  readonly cardId: number;
  readonly color?: Color;
  /** Le joueur visé par un Joker +5. */
  readonly targetId?: string;
}

/**
 * La table, sans état serveur : tout arrive par des entrées et repart par des sorties (le conteneur `Table` y branche
 * le store, la page `/dev/table` des scénarios écrits à la main). Moi en bas, les adversaires en arc en haut.
 */
@Component({
  selector: 'app-table-view',
  imports: [
    MyBadge,
    OpponentSeat,
    Piles,
    TableCenter,
    UnoActions,
    Hand,
    EffectsLayer,
    ChallengeDialog,
    ColorPicker,
    TargetPicker,
    PlusFivePrompt,
    SoundControl,
    RoundOverDialog,
    MatchOverDialog,
  ],
  providers: [AnimationDirector],
  templateUrl: './table-view.html',
  styleUrl: './table-view.scss',
})
export class TableView {
  readonly view = input.required<PlayerView>();
  /** Les dernières lignes du journal de la partie, la plus récente en dernier. */
  readonly journal = input<readonly string[]>([]);
  /** Écart entre l'horloge du serveur et celle du navigateur, pour les comptes à rebours. */
  readonly clockOffset = input(0);
  /** Résultat d'une contestation, affiché en bandeau un instant. */
  readonly banner = input<string | null>(null);
  /** Les événements de la dernière mise à jour : ils pilotent les animations (jamais une comparaison de vues). */
  readonly batch = input<EventBatch | null>(null);
  /** Les cartes posées depuis le début de la manche, pour la pile désordonnée de la défausse. */
  readonly played = input<readonly Card[]>([]);
  /** Vitesse des animations (1 = normale) : seule la démo de développement la fait varier. */
  readonly animationSpeed = input(1);

  readonly cardPlayed = output<CardPlay>();
  readonly colorChosen = output<Color>();
  readonly deckClicked = output<void>();
  readonly unoCalled = output<void>();
  readonly caught = output<string>();
  readonly penaltyAnswered = output<'accept' | 'challenge'>();
  readonly nextRoundAsked = output<void>();
  readonly left = output<void>();

  protected readonly director = inject(AnimationDirector);
  private readonly audio = inject(AudioService);
  private readonly hand = viewChild(Hand);
  /** Où était une carte de ma main : les effets en ont besoin même quand elle vient de la quitter. */
  protected readonly handRect = (cardId: number): DOMRect | null =>
    this.hand()?.lastRect(cardId) ?? null;
  private previousColor: Color | null = null;

  /** Joker choisi dans la main, en attente de sa couleur. */
  protected readonly pendingWild = signal<number | null>(null);
  /** Joker +5 choisi dans la main, en attente de sa cible. */
  protected readonly pendingTarget = signal<number | null>(null);
  /** La cible déjà choisie du Joker +5 qui attend sa couleur. */
  private readonly chosenTarget = signal<string | null>(null);
  /**
   * Une pénalité sans contestation possible me vise et le serveur ne m'offre pas le paquet pour la prendre : une fenêtre
   * propose d'accepter, la réplique se fait en cliquant une carte. Avec le cumul en échelle (`canDraw`), il n'y a pas
   * de fenêtre : le paquet prend tout, les cartes qui s'empilent sont jouables dans la main.
   */
  protected readonly plusFiveAnswer = computed(() => {
    const view = this.view();
    const options = view.me.penaltyResponse;
    return options && !options.canChallenge && !view.me.canDraw ? options : null;
  });
  /**
   * Une pénalité m'attend : mes cartes empilables sont mises en évidence dès qu'elle arrive, même si le serveur
   * n'accepte ma réponse qu'à `actionsOpenAt` (elles restent alors verrouillées).
   */
  protected readonly answeringPenalty = computed(() => this.view().me.penaltyResponse != null);
  protected readonly challengeOptions = computed(() => {
    const options = this.view().me.penaltyResponse;
    return options?.canChallenge ? options : null;
  });
  private readonly now = signal(Date.now());
  /** Écran étroit (portrait) : arc compact en haut, ou bande défilante au-delà de quatre adversaires. */
  protected readonly narrow = signal(
    typeof matchMedia === 'function' && matchMedia(NARROW_QUERY).matches,
  );
  private readonly serverNow = computed(() => this.now() + this.clockOffset());

  /** Les sièges tels qu'on les montre : les cartes d'une pioche en cours ne comptent qu'à leur arrivée. */
  private readonly seats = computed(() =>
    this.view().players.map((seat) => ({
      ...seat,
      cardCount: seat.cardCount - (this.director.unarrived().get(seat.playerId) ?? 0),
    })),
  );
  /** Ma main telle qu'on la montre : seules les cartes arrivées, celles d'une pioche en cours prennent place à leur tour. */
  protected readonly shownHand = computed(() => {
    const hidden = this.director.hiddenCardIds();
    return this.view().me.hand.filter((card) => !hidden.has(card.id));
  });
  protected readonly incomingCardIds = computed(() => {
    const hidden = this.director.hiddenCardIds();
    return this.view()
      .me.hand.filter((card) => hidden.has(card.id))
      .map((card) => card.id);
  });
  protected readonly mySeat = computed(() =>
    this.seats().find((p) => p.playerId === this.view().me.playerId),
  );
  protected readonly shownDrawPileCount = computed(
    () => this.view().drawPileCount + this.director.unlaunched(),
  );
  /**
   * Vrai quand l'effet en cours est montré en entier : jusque-là la main est neutre et le paquet muet, car le serveur
   * refuse tout coup de tour reçu avant `actionsOpenAt` (ADR 0027). Annoncer UNO et contrer restent permis.
   */
  protected readonly actionsOpen = computed(() => this.serverNow() >= this.view().actionsOpenAt);
  protected readonly myTurn = computed(
    () => this.view().currentPlayerId === this.view().me.playerId,
  );
  protected readonly opponents = computed(() =>
    opponentsInViewOrder(this.seats(), this.mySeat()?.seat ?? 0),
  );
  protected readonly strip = computed(
    () => this.narrow() && this.opponents().length > MAX_ARC_OPPONENTS,
  );
  protected readonly placements = computed(() =>
    seatLayout(this.opponents().length, this.narrow() ? 'arc' : 'table').map((placement) => ({
      ...placement,
      compact: placement.compact || this.narrow(),
    })),
  );
  private readonly stageElement = viewChild<ElementRef<HTMLElement>>('stage');
  private readonly stageSize = signal<Size>({ w: 0, h: 0 });
  private readonly viewportHeight = signal(typeof innerHeight === 'number' ? innerHeight : 0);
  /**
   * Les sièges et l'ellipse, dimensionnés d'après la zone de jeu mesurée : la table prend la place que les sièges lui
   * laissent, sans en toucher aucun. `null` tant que la zone n'est pas mesurée (le CSS donne alors une mise en page de repli).
   */
  protected readonly geometry = computed(() => {
    const stage = this.stageSize();
    return stage.w > 0 && stage.h > 0
      ? tableGeometry({
          stage,
          viewportHeight: this.viewportHeight(),
          opponents: this.opponents().length,
          shape: this.narrow() ? 'arc' : 'table',
          strip: this.strip(),
        })
      : null;
  });
  protected readonly zoneWidth = computed(() => {
    const geometry = this.geometry();
    return geometry ? geometry.ellipse.rx * 2 : null;
  });
  protected readonly maxBacks = computed(() => (this.narrow() ? NARROW_MAX_BACKS : MAX_FAN_BACKS));
  /** Part du temps de tour qu'il reste, pour l'anneau du joueur dont c'est le tour (`null` sans minuteur). */
  protected readonly turnFraction = computed(() => {
    const deadline = this.view().turnDeadline;
    const total = this.view().settings.turnTimerSeconds * 1000;
    if (deadline === null || total === 0) {
      return null;
    }
    return Math.max(0, Math.min(1, (deadline - this.serverNow()) / total));
  });
  /** La couleur à montrer : celle de la vue, sauf pendant la roue d'un Joker qui retient l'ancienne. */
  protected readonly shownColor = computed(
    () => this.director.heldColor() ?? this.view().currentColor,
  );
  protected readonly burst = computed(() => {
    const reverse = [...this.director.active()]
      .reverse()
      .find((effect) => effect.spec.kind === 'reverse');
    return reverse
      ? { id: reverse.id, durationMs: reverse.durationMs, reduced: reverse.reduced }
      : null;
  });
  protected readonly names = computed(() =>
    Object.fromEntries(this.view().players.map((seat) => [seat.playerId, seat.nickname])),
  );
  /** Mon tour touche à sa fin : l'`AudioService` en fait monter la tension de la musique (si je l'ai permis). */
  private readonly turnEnding = computed(() => {
    const seconds = this.turnSeconds();
    return (
      this.myTurn() &&
      this.actionsOpen() &&
      seconds !== null &&
      seconds > 0 &&
      seconds <= TENSION_SECONDS
    );
  });
  protected readonly currentName = computed(() => this.nameOf(this.view().currentPlayerId));
  protected readonly turnSeconds = computed(() => this.secondsUntil(this.view().turnDeadline));
  protected readonly nextRoundSeconds = computed(() =>
    this.secondsUntil(this.view().nextRoundDeadline),
  );
  protected readonly lastLines = computed(() => this.journal().slice(-JOURNAL_LINES));

  /** Les fenêtres de contre-UNO ouvertes sur d'autres joueurs que moi, avec ce qu'il reste de leur temps. */
  private readonly otherWindows = computed(() =>
    this.view().unoWindows.filter(
      (window) =>
        window.targetId !== this.view().me.playerId && this.serverNow() < window.expiresAt,
    ),
  );
  protected readonly forgotten = computed(
    () => new Set(this.otherWindows().map((window) => window.targetId)),
  );
  protected readonly catchButtons = computed<readonly CatchButton[]>(() =>
    this.otherWindows().map((window) => {
      const inGrace = this.serverNow() < window.graceEndsAt;
      const end = inGrace ? window.graceEndsAt : window.expiresAt;
      return {
        playerId: window.targetId,
        nickname: this.nameOf(window.targetId),
        inGrace,
        secondsLeft: Math.max(0, Math.ceil((end - this.serverNow()) / 1000)),
      };
    }),
  );

  constructor() {
    effect(() => this.audio.setTension(this.turnEnding()));
    inject(DestroyRef).onDestroy(() => this.audio.setTension(false));
    // Le lot d'événements pilote les effets ; la couleur d'avant la mise à jour est retenue le temps d'une roue
    effect(() => {
      const batch = this.batch();
      if (batch) {
        untracked(() =>
          this.director.enqueue(batch.events, {
            resync: batch.resync,
            previousColor: this.previousColor,
            drawStepMs: this.view().drawStepMs,
            meId: this.view().me.playerId,
          }),
        );
      }
    });
    effect(() => this.director.speed.set(this.animationSpeed()));
    effect((onCleanup) => {
      const element = this.stageElement()?.nativeElement;
      if (!element || typeof ResizeObserver !== 'function') {
        return;
      }
      const measure = (): void => {
        const box = element.getBoundingClientRect();
        this.stageSize.set({ w: box.width, h: box.height });
        this.viewportHeight.set(innerHeight);
      };
      const observer = new ResizeObserver(measure);
      observer.observe(element);
      measure();
      onCleanup(() => observer.disconnect());
    });
    effect(() => {
      const color = this.view().currentColor;
      untracked(() => (this.previousColor = color));
    });
    // La main s'ouvre à l'heure dite, sans attendre le prochain battement de l'horloge
    effect((onCleanup) => {
      const wait = this.view().actionsOpenAt - this.serverNow();
      if (wait > 0) {
        const timer = setTimeout(() => this.now.set(Date.now()), wait);
        onCleanup(() => clearTimeout(timer));
      }
    });
    effect((onCleanup) => {
      const timer = setInterval(() => this.now.set(Date.now()), CLOCK_TICK_MS);
      onCleanup(() => clearInterval(timer));
    });
    effect((onCleanup) => {
      if (typeof matchMedia !== 'function') {
        return;
      }
      const query = matchMedia(NARROW_QUERY);
      const update = (): void => this.narrow.set(query.matches);
      query.addEventListener('change', update);
      update();
      onCleanup(() => query.removeEventListener('change', update));
    });
  }

  protected play(cardId: number): void {
    const card = this.view().me.hand.find((c) => c.id === cardId);
    if (card?.rank === 'wildDrawFive') {
      this.pendingTarget.set(cardId);
    } else if (card?.color === null) {
      this.pendingWild.set(cardId);
    } else {
      this.cardPlayed.emit({ cardId });
    }
  }

  /** La cible du Joker +5 est choisie (dans la liste ou sur un siège) : reste sa couleur. */
  protected chooseTarget(playerId: string): void {
    const cardId = this.pendingTarget();
    if (cardId === null) {
      return;
    }
    this.chosenTarget.set(playerId);
    this.pendingTarget.set(null);
    this.pendingWild.set(cardId);
  }

  protected cancelPlay(): void {
    this.pendingTarget.set(null);
    this.pendingWild.set(null);
    this.chosenTarget.set(null);
  }

  protected playWild(color: Color): void {
    const cardId = this.pendingWild();
    const targetId = this.chosenTarget() ?? undefined;
    this.cancelPlay();
    if (cardId !== null) {
      this.cardPlayed.emit(
        targetId === undefined ? { cardId, color } : { cardId, color, targetId },
      );
    }
  }

  private nameOf(playerId: string): string {
    return this.view().players.find((p) => p.playerId === playerId)?.nickname ?? '?';
  }

  /** Secondes restantes avant une échéance donnée en heure du serveur. */
  private secondsUntil(deadline: number | null): number | null {
    return deadline === null ? null : Math.max(0, Math.ceil((deadline - this.serverNow()) / 1000));
  }
}
