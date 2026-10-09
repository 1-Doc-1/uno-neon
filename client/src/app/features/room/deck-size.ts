import type { CardMultiplier } from '../../protocol/generated/protocol';

/** Le nombre de cartes d'un paquet pour ces multiplicateurs (SPEC §3) : seulement pour l'afficher, le serveur compose le paquet. */
export function deckSize(multipliers: {
  readonly drawTwoMultiplier: CardMultiplier;
  readonly wildDrawFourMultiplier: CardMultiplier;
  readonly wildDrawFiveMultiplier: CardMultiplier;
}): number {
  const colors = 4;
  const perColor = 1 + 2 * 11 + 2 * multipliers.drawTwoMultiplier; // un 0, deux de chaque 1-9, Passe, Inversion, +2 × m
  const wilds = 4;
  return (
    colors * perColor +
    wilds +
    4 * multipliers.wildDrawFourMultiplier +
    2 * multipliers.wildDrawFiveMultiplier
  );
}
