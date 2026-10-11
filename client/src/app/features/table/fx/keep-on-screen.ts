import {
  afterNextRender,
  computed,
  Directive,
  DOCUMENT,
  ElementRef,
  inject,
  input,
  signal,
} from '@angular/core';

/** Marge entre un effet et le bord de l'écran. */
export const SCREEN_MARGIN_PX = 8;

/**
 * Le centre d'un effet de `size` pixels de large (ou de haut), ramené dans l'écran : jamais au-delà des bords, et au
 * milieu quand l'effet est plus grand que l'écran. Pure, pour être testée sans navigateur.
 */
export function keepInside(center: number, size: number, screen: number): number {
  const half = size / 2 + SCREEN_MARGIN_PX;
  return half * 2 > screen ? screen / 2 : Math.max(half, Math.min(screen - half, center));
}

/**
 * Pose un effet centré sur un point (le siège de la cible, le centre de la table…) sans jamais le laisser sortir de
 * l'écran : un téléphone est trop étroit pour un grand texte posé sur un siège de côté. La taille est mesurée une fois
 * l'effet affiché (sa taille de mise en page, sans l'échelle de son animation).
 */
@Directive({
  selector: '[appKeepOnScreen]',
  host: { '[style.left.px]': 'left()', '[style.top.px]': 'top()' },
})
export class KeepOnScreen {
  readonly anchorX = input.required<number>();
  readonly anchorY = input.required<number>();

  private readonly document = inject(DOCUMENT);
  private readonly host = inject<ElementRef<HTMLElement>>(ElementRef);
  private readonly size = signal({ width: 0, height: 0 });

  protected readonly left = computed(() =>
    keepInside(this.anchorX(), this.size().width, this.document.documentElement.clientWidth),
  );
  protected readonly top = computed(() =>
    keepInside(this.anchorY(), this.size().height, this.document.documentElement.clientHeight),
  );

  /** La plus grande largeur et hauteur de l'effet ou de l'un de ses éléments (une légende peut dépasser son tampon). */
  private measure(): { width: number; height: number } {
    const host = this.host.nativeElement;
    let width = host.offsetWidth;
    let height = host.offsetHeight;
    for (const child of Array.from(host.querySelectorAll<HTMLElement>('*'))) {
      width = Math.max(width, child.offsetWidth ?? 0);
      height = Math.max(height, child.offsetHeight ?? 0);
    }
    return { width, height };
  }

  constructor() {
    afterNextRender(() => this.size.set(this.measure()));
  }
}
