import { Component, DestroyRef, ElementRef, inject, viewChild } from '@angular/core';
import { type Burst, ParticleSystem } from './particle-system';

/** Le plus long pas simulé : un onglet resté en arrière-plan ne doit pas faire sauter les grains. */
const MAX_STEP_SECONDS = 0.05;

/**
 * Une seule couche `<canvas>` pour toute la poussière des effets. La boucle `requestAnimationFrame` ne tourne que
 * tant qu'il reste une particule vivante : elle démarre à la première et s'arrête (canvas effacé) à la dernière.
 */
@Component({
  selector: 'app-particle-canvas',
  template: '<canvas #canvas></canvas>',
  styles: `
    :host {
      position: absolute;
      inset: 0;
      pointer-events: none;
    }
    canvas {
      display: block;
      width: 100%;
      height: 100%;
    }
  `,
  host: { 'aria-hidden': 'true' },
})
export class ParticleCanvas {
  private readonly canvas = viewChild.required<ElementRef<HTMLCanvasElement>>('canvas');
  private readonly system = new ParticleSystem();
  private frame: number | null = null;
  private last = 0;

  constructor() {
    inject(DestroyRef).onDestroy(() => this.stopLoop());
  }

  /** La boucle d'animation tourne-t-elle ? (Les tests vérifient qu'elle s'arrête.) */
  get running(): boolean {
    return this.frame !== null;
  }

  get alive(): number {
    return this.system.alive;
  }

  burst(burst: Burst): void {
    this.system.burst(burst);
    if (this.frame === null && this.system.alive > 0) {
      this.fit();
      this.last = performance.now();
      this.frame = requestAnimationFrame((now) => this.tick(now));
    }
  }

  private tick(now: number): void {
    const seconds = Math.min(MAX_STEP_SECONDS, (now - this.last) / 1000);
    this.last = now;
    this.system.step(seconds);
    this.draw();
    if (this.system.alive > 0) {
      this.frame = requestAnimationFrame((time) => this.tick(time));
    } else {
      this.frame = null;
      this.clear();
    }
  }

  private fit(): void {
    const canvas = this.canvas().nativeElement;
    const { width, height } = canvas.getBoundingClientRect();
    if (canvas.width !== Math.round(width) || canvas.height !== Math.round(height)) {
      canvas.width = Math.round(width);
      canvas.height = Math.round(height);
    }
  }

  private draw(): void {
    const canvas = this.canvas().nativeElement;
    const context = canvas.getContext('2d');
    if (!context) {
      return;
    }
    context.clearRect(0, 0, canvas.width, canvas.height);
    for (const particle of this.system.particles) {
      context.globalAlpha = Math.max(0, 1 - particle.age / particle.life) * 0.75;
      context.fillStyle = particle.color;
      context.beginPath();
      context.arc(particle.x, particle.y, particle.size, 0, Math.PI * 2);
      context.fill();
    }
    context.globalAlpha = 1;
  }

  private clear(): void {
    const canvas = this.canvas().nativeElement;
    canvas.getContext('2d')?.clearRect(0, 0, canvas.width, canvas.height);
  }

  private stopLoop(): void {
    if (this.frame !== null) {
      cancelAnimationFrame(this.frame);
      this.frame = null;
    }
  }
}
