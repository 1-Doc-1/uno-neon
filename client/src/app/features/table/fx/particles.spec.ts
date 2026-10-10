import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { ParticleCanvas } from './particle-canvas';
import { MAX_PARTICLES, ParticleSystem } from './particle-system';

describe('ParticleSystem', () => {
  const burst = { x: 50, y: 50, color: 'red', count: 30 };

  it('never holds more than 40 particles, however many are asked for', () => {
    const system = new ParticleSystem();

    system.burst(burst);
    system.burst(burst);
    system.burst({ ...burst, count: 500 });

    expect(system.alive).toBe(MAX_PARTICLES);
    expect(MAX_PARTICLES).toBe(40);
  });

  it('spreads the particles from where the burst happened', () => {
    const system = new ParticleSystem(() => 0.5);

    system.burst({ ...burst, count: 3 });

    expect(system.particles.every((particle) => particle.x === 50 && particle.y === 50)).toBe(true);
    system.step(0.1);
    expect(system.particles.every((particle) => particle.x !== 50)).toBe(true);
  });

  it('lets every particle die: nothing is left after the longest life', () => {
    const system = new ParticleSystem();
    system.burst(burst);

    system.step(0.5);
    expect(system.alive).toBeGreaterThan(0);
    system.step(2);

    expect(system.alive).toBe(0);
  });
});

describe('ParticleCanvas', () => {
  let frames: Map<number, (time: number) => void>;
  let cancelledIds: number[];
  let nextId: number;
  let clock: number;

  beforeEach(() => {
    frames = new Map();
    cancelledIds = [];
    nextId = 1;
    vi.stubGlobal('requestAnimationFrame', (callback: (time: number) => void) => {
      frames.set(nextId, callback);
      return nextId++;
    });
    vi.stubGlobal('cancelAnimationFrame', (id: number) => {
      cancelledIds.push(id);
      frames.delete(id);
    });
  });

  afterEach(() => vi.unstubAllGlobals());

  /** Rend le composant puis oublie les images demandées par Angular lui-même : seules celles des particules comptent. */
  function render() {
    const fixture = TestBed.createComponent(ParticleCanvas);
    fixture.detectChanges();
    frames.clear();
    clock = performance.now();
    return fixture;
  }

  /** Joue la prochaine image de la boucle des particules, `milliseconds` plus tard. */
  const frame = (milliseconds: number) => {
    clock += milliseconds;
    const [id, callback] = [...frames.entries()][0] ?? [];
    if (id !== undefined) {
      frames.delete(id);
      callback?.(clock);
    }
  };

  it('does not run a loop while nothing happens', () => {
    const canvas = render().componentInstance;

    expect(canvas.running).toBe(false);
    expect(frames.size).toBe(0);
  });

  it('runs while particles live, then stops by itself when nothing moves any more', () => {
    const canvas = render().componentInstance;

    canvas.burst({ x: 10, y: 10, color: 'red', count: 20 });
    expect(canvas.running).toBe(true);

    for (let image = 0; image < 200 && canvas.running; image++) {
      frame(50);
    }

    expect(canvas.alive).toBe(0);
    expect(canvas.running).toBe(false);
    expect(frames.size).toBe(0);
  });

  it('keeps a single loop when a second burst comes while the first still lives', () => {
    const canvas = render().componentInstance;

    canvas.burst({ x: 10, y: 10, color: 'red', count: 10 });
    canvas.burst({ x: 20, y: 20, color: 'blue', count: 10 });

    expect(frames.size).toBe(1);
  });

  it('cancels its loop when it is destroyed', () => {
    const fixture = render();
    fixture.componentInstance.burst({ x: 1, y: 1, color: 'red', count: 5 });
    const [id] = [...frames.keys()];

    fixture.destroy();

    expect(id).toBeDefined();
    expect(cancelledIds).toContain(id);
  });
});
