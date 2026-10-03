// Les graines des tests. Chacune a été trouvée avec `npm run seeds` (tools/find-seeds.ts), qui joue la même politique
// que les tests par le protocole ; ce qu'elle garantit est écrit à côté. Dans une partie à deux, Bob (l'invité) joue
// toujours le premier : Alice (l'hôte) est la donneuse. Si le moteur change sa façon de mélanger ou de distribuer, ces
// graines ne donneront plus les mêmes parties : relancer le chercheur et les mettre à jour.
export const SEEDS = {
  /**
   * Défausse : 3 vert. Main de Bob : 6 rouge, Joker, Inversion verte, Passe-tour bleu, Passe-tour vert, 6 bleu, 5 vert.
   * Avec la politique des tests, Bob gagne en manche unique (version 11) après être passé à une carte (version 10).
   */
  quickGame: 1,
  /** Défausse : 2 vert. Bob a un Joker (et 4 cartes jouables en tout, dont le 6 vert). */
  jokerInHand: 5,
  /** Défausse : 5 rouge. Bob a un +4 et aussi une carte rouge : poser le +4 est un bluff. */
  bluffedWildDrawFour: 2,
  /** Défausse : 4 rouge. Bob a un +4 et aucune carte rouge : poser le +4 est légal. */
  legalWildDrawFour: 110,
} as const;
