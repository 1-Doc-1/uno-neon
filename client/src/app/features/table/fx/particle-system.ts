/** Au plus 40 particules à l'écran, quoi qu'il arrive (SPEC §13.4). */
export const MAX_PARTICLES = 40;

export interface Particle {
  x: number;
  y: number;
  vx: number;
  vy: number;
  /** Rayon en pixels. */
  size: number;
  /** Durée de vie totale et âge, en secondes. */
  life: number;
  age: number;
  /** Une valeur CSS (issue d'un token), jamais une couleur en dur. */
  color: string;
}

export interface Burst {
  readonly x: number;
  readonly y: number;
  readonly color: string;
  readonly count: number;
}

const GRAVITY = -18;
const DRAG = 1.6;

/**
 * La « poussière » des effets : un nuage de grains qui jaillit, ralentit, s'élève et s'éteint. Logique pure (pas de
 * canvas, pas d'horloge) : `ParticleCanvas` la fait avancer. `random` est injectable pour les tests.
 */
export class ParticleSystem {
  readonly particles: Particle[] = [];

  constructor(private readonly random: () => number = Math.random) {}

  get alive(): number {
    return this.particles.length;
  }

  /** Lance un nuage ; ce qui dépasse le plafond de 40 particules n'est pas créé. */
  burst({ x, y, color, count }: Burst): void {
    const room = MAX_PARTICLES - this.particles.length;
    for (let index = 0; index < Math.min(count, room); index++) {
      const angle = this.random() * Math.PI * 2;
      const speed = 30 + this.random() * 90;
      this.particles.push({
        x,
        y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: 1.5 + this.random() * 2.5,
        life: 0.7 + this.random() * 0.7,
        age: 0,
        color,
      });
    }
  }

  /** Avance de `seconds` : les grains avancent, ralentissent, montent doucement et disparaissent à la fin de leur vie. */
  step(seconds: number): void {
    for (const particle of this.particles) {
      particle.age += seconds;
      particle.vx -= particle.vx * DRAG * seconds;
      particle.vy -= particle.vy * DRAG * seconds;
      particle.vy += GRAVITY * seconds;
      particle.x += particle.vx * seconds;
      particle.y += particle.vy * seconds;
    }
    for (let index = this.particles.length - 1; index >= 0; index--) {
      const particle = this.particles[index];
      if (particle && particle.age >= particle.life) {
        this.particles.splice(index, 1);
      }
    }
  }
}
