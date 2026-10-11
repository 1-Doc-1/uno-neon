// Les graines des tests. Chacune a Ã©tÃ© trouvÃ©e avec `npm run seeds` (tools/find-seeds.ts), qui joue la mÃªme politique
// que les tests par le protocole ; ce qu'elle garantit est Ã©crit Ã  cÃ´tÃ©. Dans une partie Ã  deux, Bob (l'invitÃ©) joue
// toujours le premier (sauf mention contraire) : Alice (l'hÃ´te) est la donneuse. Si le moteur change sa faÃ§on de
// mÃ©langer ou de distribuer, ou la composition du paquet (ADR 0028 : 110 cartes), ces graines ne donneront plus les
// mÃªmes parties : relancer le chercheur et les mettre Ã  jour. Une plage se trie vite par la donne seule
// (UNO_FIND_DEAL_ONLY=1), puis on rejoue les candidats en entier (UNO_FIND_SEEDS=â€¦).
export const SEEDS = {
  /**
   * DÃ©fausse : 8 vert. Bob joue en premier et n'a qu'une seule carte jouable : toutes les autres sont dÃ©sactivÃ©es. Sert
   * aux scÃ©narios qui ne dÃ©pendent que de cela.
   */
  quickGame: 35,
  /**
   * Une partie courte (la politique joue sur la main triÃ©e, `sortHand`) : Bob joue en premier, passe Ã  une carte jouable Ã  la
   * version 9 et gagne la manche unique Ã  la version 11.
   */
  fastGame: 126,
  /**
   * Pioche guidÃ©e : Bob joue sa premiÃ¨re carte jouable, et Alice n'a rien Ã  rÃ©pondre (le serveur pioche pour elle, version 2).
   */
  guidedDraw: 12,
  /** DÃ©fausse : 2 jaune. Bob a un Joker et plusieurs cartes jouables, Alice de quoi rÃ©pondre au bleu. */
  jokerInHand: 96,
  /** DÃ©fausse : 7 rouge. Bob a un +4 et aussi des cartes rouges (1 rouge, Inversion rouge) : poser le +4 est un bluff. */
  bluffedWildDrawFour: 30,
  /** DÃ©fausse : 6 rouge. Bob a un +4 et aucune carte rouge : poser le +4 est lÃ©gal. */
  legalWildDrawFour: 27,
  /**
   * Pioche Â« jusqu'Ã  pouvoir jouer Â» (ADR 0024) : Alice n'a rien Ã  jouer et pioche plusieurs cartes d'un coup, la derniÃ¨re
   * Ã©tant spÃ©ciale : elle doit choisir, l'Ã©cran reste figÃ©. Alice se retrouve avec 9 cartes. (TrouvÃ©e avec
   * UNO_FIND_DRAW_AMOUNT=untilPlayable.)
   */
  multiDraw: 45,
  /**
   * DÃ©fausse : 6 bleu. Bob a deux +2 bleus jouables : Ã  deux joueurs, un +2 fait sauter Alice, et Bob rejouerait aussitÃ´t si
   * le serveur n'attendait pas la fin de la distribution (ADR 0027).
   */
  drawTwoFirst: 91,
  /**
   * Joker +5 (ADR 0028, 0034), paquet Ã  Ã—5 Joker +5 : Bob, qui joue en premier, a un seul Joker +5 ; Alice n'en a aucun.
   * DÃ©fausse : 4 jaune. (UNO_FIND_SETTINGS='{"wildDrawFiveMultiplier":5}'.)
   */
  plusFiveSimple: 5,
  /** MÃªme rÃ©glage : Bob et Alice ont chacun un Joker +5, qu'Alice ne peut pourtant pas jouer en rÃ©ponse. DÃ©fausse : 7 jaune. */
  plusFiveAnswered: 21,
  /**
   * Cumul Ã  l'Ã©chelle (ADR 0029), paquet Ã  Ã—5 +2 et Ã—5 +4 (UNO_FIND_SETTINGS='{"stacking":"ladder","drawTwoMultiplier":5,"wildDrawFourMultiplier":5}') :
   * Bob joue en premier sur un 5 jaune, avec un +2 jaune jouable et un +4. Alice a un +2 et ni +4 ni +5.
   */
  ladderChain: 341,
} as const;
