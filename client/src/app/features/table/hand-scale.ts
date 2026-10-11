/** Jusqu'à ce nombre de cartes, la main garde sa taille normale : elle se resserre seulement (chevauchement). */
export const FULL_SIZE_UP_TO = 16;
/** Pas minimal entre deux cartes, en largeurs de carte : en dessous, l'indice de la carte n'est plus lisible. */
const MIN_STEP_RATIO = 0.4;
const MIN_SCALE = 0.3;

/**
 * Échelle de la main (entre `MIN_SCALE` et 1) : au-delà de 16 cartes, on réduit les cartes pour que chacune garde un pas
 * lisible dans la largeur disponible, sans défilement.
 */
export function handScale(
  count: number,
  availableWidth: number,
  cardWidth: number,
  gutter: number,
): number {
  if (count <= FULL_SIZE_UP_TO || availableWidth <= 0) {
    return 1;
  }
  const needed = cardWidth * (1 + MIN_STEP_RATIO * (count - 1)) + 2 * gutter;
  return Math.min(1, Math.max(MIN_SCALE, availableWidth / needed));
}
