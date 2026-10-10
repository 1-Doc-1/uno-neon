// Les graines des tests. Chacune a été trouvée avec `npm run seeds` (tools/find-seeds.ts), qui joue la même politique
// que les tests par le protocole ; ce qu'elle garantit est écrit à côté. Dans une partie à deux, Bob (l'invité) joue
// toujours le premier (sauf mention contraire) : Alice (l'hôte) est la donneuse. Si le moteur change sa façon de
// mélanger ou de distribuer, ou la composition du paquet (ADR 0028 : 110 cartes), ces graines ne donneront plus les
// mêmes parties : relancer le chercheur et les mettre à jour.
export const SEEDS = {
  /**
   * Défausse : 7 jaune. Main de Bob : 8 vert, 1 bleu, Passe-tour vert, 1 jaune, 8 rouge, 0 vert, 5 bleu : seul le 1 jaune
   * est jouable. Alice n'a ni jaune ni 1 (le serveur pioche pour elle). Avec la politique des tests, Bob passe à une
   * carte (version 29, jouable) et gagne en manche unique (version 31).
   */
  quickGame: 39,
  /**
   * Une partie courte : Bob passe à une carte jouable à la version 12 et gagne la manche unique à la version 14 (Alice n'a
   * rien à jouer à la version 4). Défausse : 6 bleu. Main de Bob : Passe-tour bleu, 3 rouge, 6 vert, Joker, +2 bleu, 8 rouge, 2 vert.
   */
  fastGame: 109,
  /** Défausse : 8 jaune. Bob a un Joker en tête de main et 3 cartes jouables. */
  jokerInHand: 9,
  /** Défausse : 7 vert. Bob a un +4 et aussi une carte verte (1 vert) : poser le +4 est un bluff. */
  bluffedWildDrawFour: 34,
  /** Défausse : 8 rouge. Bob a un +4 et aucune carte rouge : poser le +4 est légal. */
  legalWildDrawFour: 30,
  /**
   * Pioche « jusqu'à pouvoir jouer » (ADR 0024) : défausse 2 rouge, Alice joue en premier. À la version 6, Alice n'a rien
   * à jouer et pioche 4 cartes d'un coup, la dernière étant spéciale : elle doit choisir, l'écran reste figé.
   * Alice se retrouve avec 9 cartes. (Trouvée avec UNO_FIND_DRAW_AMOUNT=untilPlayable.)
   */
  multiDraw: 21,
  /**
   * Défausse : 0 bleu. Bob a un +2 bleu jouable : à deux joueurs, un +2 fait sauter Alice, et Bob rejouerait aussitôt si
   * le serveur n'attendait pas la fin de la distribution (ADR 0027).
   */
  drawTwoFirst: 94,
  /**
   * Joker +5 (ADR 0028), paquet à ×5 Joker +5 : Bob, qui joue en premier, a un seul Joker +5 (et un Joker +4, un Joker et
   * un Inversion rouge) ; Alice n'en a aucun. Défausse : 1 bleu. (UNO_FIND_SETTINGS='{"wildDrawFiveMultiplier":5}'.)
   */
  plusFiveSimple: 1,
  /** Même réglage : Bob et Alice ont chacun exactement un Joker +5. Défausse : 6 rouge. */
  plusFiveAnswered: 23,
  /**
   * Cumul à l'échelle (ADR 0029), paquet à ×5 +2 et ×5 +4 (UNO_FIND_SETTINGS='{"stacking":"ladder","drawTwoMultiplier":5,"wildDrawFourMultiplier":5}') :
   * Bob joue en premier sur un 0 jaune, avec un +2 jaune jouable, un autre +2 rouge et deux +4. Alice a un +2 et ni +4 ni +5.
   */
  ladderChain: 54,
} as const;
