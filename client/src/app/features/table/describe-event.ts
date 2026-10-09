import type { ClientEvent } from '../../protocol/generated/protocol';
import { cardLabel, COLOR_NAME, SHAPE_CHAR } from '../../ui/color-meta';

/** Phrase du journal pour un événement, ou `null` si l'événement n'a rien à raconter (ex. changement de tour). */
export function describeEvent(
  event: ClientEvent,
  nameOf: (playerId: string) => string,
): string | null {
  switch (event.kind) {
    case 'roundStarted':
      return `Manche ${event.round} : ${nameOf(event.dealerId)} distribue.`;
    case 'cardPlayed': {
      const color = event.chosenColor
        ? ` → ${COLOR_NAME[event.chosenColor]} ${SHAPE_CHAR[event.chosenColor]}`
        : '';
      const shape = event.card.color ? ` ${SHAPE_CHAR[event.card.color]}` : '';
      return `${nameOf(event.playerId)} pose ${cardLabel(event.card)}${shape}${color}.`;
    }
    case 'cardsDrawn':
      return `${nameOf(event.playerId)} pioche ${event.count} carte${event.count > 1 ? 's' : ''}.`;
    case 'playerSkipped':
      return `${nameOf(event.playerId)} passe son tour.`;
    case 'directionChanged':
      return 'Le sens du jeu s’inverse.';
    case 'colorChosen':
      return `${nameOf(event.playerId)} choisit ${COLOR_NAME[event.color]} ${SHAPE_CHAR[event.color]}.`;
    case 'penaltyStacked':
      return `${nameOf(event.playerId)} empile : ${event.pendingDraw} cartes à piocher.`;
    case 'plusFiveTargeted':
      return `${nameOf(event.playerId)} vise ${nameOf(event.targetId)} : ${event.total} cartes à piocher.`;
    case 'challengeResolved':
      return event.wasBluff
        ? `${nameOf(event.challengerId)} conteste : c’était un bluff, ${nameOf(event.penalizedPlayerId)} pioche ${event.penaltyAmount}.`
        : `${nameOf(event.challengerId)} conteste à tort et pioche ${event.penaltyAmount}.`;
    case 'unoCalled':
      return `${nameOf(event.playerId)} annonce UNO !`;
    case 'unoCaught':
      return `${nameOf(event.catcherId)} contre ${nameOf(event.targetId)} : ${event.penaltyAmount} cartes.`;
    case 'handsSwapped':
      return `${nameOf(event.playerId)} échange sa main avec ${nameOf(event.targetId)}.`;
    case 'handsRotated':
      return 'Les mains tournent.';
    case 'deckReshuffled':
      return 'La défausse est remélangée dans la pioche.';
    case 'roundEnded':
      return `${nameOf(event.winnerId)} gagne la manche (+${event.points}).`;
    case 'matchEnded':
      return `${nameOf(event.winnerId)} gagne la partie !`;
    case 'playerDisconnected':
      return `${nameOf(event.playerId)} s’est déconnecté.`;
    case 'playerReconnected':
      return `${nameOf(event.playerId)} est de retour.`;
    case 'hostChanged':
      return `${nameOf(event.playerId)} devient l’hôte.`;
    case 'turnChanged':
      return null;
  }
}
