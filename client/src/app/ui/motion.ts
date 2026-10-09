/**
 * Toutes les durées des animations (SPEC §13), en millisecondes. Les animations ont toujours la même vitesse : une seule
 * source, rien d'autre dans le client ne code une durée d'animation en dur.
 */
export const MOTION_MS = {
  /** Une carte posée glisse jusqu'à la défausse, s'y pose avec un léger rebond… */
  playFlight: 700,
  /** …puis reste visible un instant avant l'action suivante. */
  playRest: 380,
  /** Le vol d'une carte piochée. L'écart entre deux cartes, lui, vient du serveur (`drawStepMs`, ADR 0026). */
  drawFlight: 520,
  /** Effets spéciaux (+2, +4, Passe, Inversion, Joker) : 1 à 1,4 s. */
  bigText: 1200,
  skip: 1000,
  reverse: 1400,
  wheel: 1200,
  challenge: 1200,
  spotlight: 1400,
  uno: 1100,
  caught: 1100,
  turn: 500,
  /** Pause entre deux effets spéciaux ou tampons. */
  specialGap: 250,
  /** En mouvement réduit : un simple fondu. */
  reduced: 160,
  /** Au-delà de ce retard (à vitesse normale) la file est abandonnée au profit de l'état final. */
  maxBacklog: 9000,
} as const;

/** Au-delà de ce nombre d'étapes en attente, la file accélère pour rattraper son retard au lieu de l'accumuler. */
export const CATCH_UP_THRESHOLD = 3;
export const CATCH_UP_MIN_FACTOR = 0.4;
