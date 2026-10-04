import { afterNextRender, Directive, ElementRef, inject, input } from '@angular/core';
import type { Flight } from './effect-geometry';

const EASING = 'cubic-bezier(0.22, 0.8, 0.26, 1)';
const SETTLE_SCALE = 1.07;

function transformAt(
  box: { cx: number; cy: number },
  flight: Flight,
  tilt: number,
  scale: number,
): string {
  const { w, h } = flight.to;
  return `translate(${box.cx - w / 2}px, ${box.cy - h / 2}px) scale(${scale}) rotate(${tilt}deg)`;
}

/**
 * Le vol d'une carte, par la technique FLIP : l'élément est posé à son arrivée (Last), on calcule l'écart avec le départ
 * (First), on part de cet écart (Invert) et on laisse l'animation Web le résorber (Play). Seuls `transform` et `opacity`
 * bougent. En mouvement réduit, la carte reste à l'arrivée et apparaît puis disparaît en fondu.
 */
@Directive({ selector: '[appFlightMotion]' })
export class FlightMotion {
  readonly flight = input.required<Flight>();
  readonly reduced = input(false);

  constructor() {
    const element: HTMLElement = inject(ElementRef).nativeElement;
    afterNextRender(() => {
      if (typeof element.animate !== 'function') {
        return;
      }
      const flight = this.flight();
      const last = transformAt(flight.to, flight, flight.toTilt, 1);
      if (this.reduced()) {
        element.animate(
          [
            { transform: last, opacity: 0 },
            { transform: last, opacity: 1, offset: 0.35 },
            { transform: last, opacity: 1, offset: 0.7 },
            { transform: last, opacity: 0 },
          ],
          { duration: flight.durationMs, delay: flight.delayMs, fill: 'both' },
        );
        return;
      }
      const first = transformAt(flight.from, flight, flight.fromTilt, flight.from.w / flight.to.w);
      // Une carte posée glisse puis rebondit un peu en arrivant (dernier cinquième de la durée)
      const frames: Keyframe[] = flight.settle
        ? [
            { transform: first, easing: EASING },
            { transform: last, offset: 0.8 },
            { transform: transformAt(flight.to, flight, flight.toTilt, SETTLE_SCALE), offset: 0.9 },
            { transform: last },
          ]
        : [{ transform: first }, { transform: last }];
      element.animate(frames, {
        duration: flight.durationMs,
        delay: flight.delayMs,
        easing: flight.settle ? 'linear' : EASING,
        fill: 'both',
      });
    });
  }
}
