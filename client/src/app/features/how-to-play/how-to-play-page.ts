import { Component } from '@angular/core';
import { RouterLink } from '@angular/router';
import type { Card } from '../../protocol/generated/protocol';
import { CardFace } from '../../ui/card';
import { Logo } from '../../ui/logo';

interface SpecialCard {
  readonly card: Card;
  readonly name: string;
  readonly text: string;
}

interface HouseRule {
  readonly name: string;
  readonly text: string;
  readonly example: string;
}

/** Les cartes montrées en exemple : leurs identifiants ne servent qu'à les distinguer. */
const SPECIAL_CARDS: readonly SpecialCard[] = [
  {
    card: { id: 1, color: 'blue', rank: 'drawTwo' },
    name: '+2',
    text: 'Le joueur suivant pioche 2 cartes et passe son tour.',
  },
  {
    card: { id: 2, color: 'green', rank: 'skip' },
    name: 'Passe',
    text: 'Le joueur suivant passe son tour.',
  },
  {
    card: { id: 3, color: 'red', rank: 'reverse' },
    name: 'Inversion',
    text: 'Le sens du jeu s’inverse. À deux joueurs, elle fait passer le tour de l’autre.',
  },
  {
    card: { id: 4, color: null, rank: 'wild' },
    name: 'Joker',
    text: 'Se pose sur n’importe quelle carte. Tu choisis la nouvelle couleur.',
  },
  {
    card: { id: 5, color: null, rank: 'wildDrawFour' },
    name: 'Joker +4',
    text: 'Tu choisis la couleur et le joueur suivant pioche 4 cartes. Tu ne devrais le poser que si tu n’as aucune carte de la couleur en cours : sinon, le joueur visé peut te contester (s’il a raison, tu piocheras 4 cartes ; s’il a tort, il en pioche 6).',
  },
  {
    card: { id: 6, color: null, rank: 'wildDrawFive' },
    name: 'Joker +5 (doré)',
    text: 'Tu choisis la couleur et un joueur de ton choix, qui pioche 5 cartes. Il peut te répondre avec un autre Joker +5 : le total monte à 10, 15…, jusqu’à ce que quelqu’un accepte.',
  },
];

const HOUSE_RULES: readonly HouseRule[] = [
  {
    name: 'Durée de la partie',
    text: 'Une seule manche, ou la première personne à 250 ou 500 points gagne.',
    example: 'À 250 points, trois bonnes manches suffisent souvent à finir la partie.',
  },
  {
    name: 'Minuteur par tour',
    text: 'Le temps dont tu disposes pour jouer ; passé ce délai, le jeu joue à ta place.',
    example: 'Avec 15 s, un joueur qui hésite voit sa première carte jouable posée pour lui.',
  },
  {
    name: 'Joueurs maximum',
    text: 'Le nombre de places du salon (2 à 10).',
    example: 'Mets 4 pour une partie entre quatre amis, sans que personne d’autre n’entre.',
  },
  {
    name: 'Règle de pioche : Guidée',
    text: 'Le jeu ne te laisse piocher que quand c’est utile : sans carte jouable, ou pour garder un +2 ou un Joker.',
    example: 'Tu as une carte jouable normale : le paquet est fermé, il faut la poser.',
  },
  {
    name: 'Règle de pioche : Officielle',
    text: 'On peut toujours piocher au lieu de jouer, même avec une carte jouable.',
    example: 'Tu as un 7 rouge jouable mais tu préfères le garder : tu pioches quand même.',
  },
  {
    name: 'Pioche : jusqu’à pouvoir jouer',
    text: 'Sans carte jouable, tu pioches jusqu’à tomber sur une carte que tu peux poser.',
    example: 'Il te faut un bleu : tu pioches un jaune, un vert, puis un bleu, que tu peux poser.',
  },
  {
    name: 'Pioche : 1 carte',
    text: 'Sans carte jouable, tu ne pioches qu’une seule carte.',
    example: 'Tu pioches un jaune alors qu’il te faut un bleu : ton tour est fini.',
  },
  {
    name: 'Dernière carte : UNO obligatoire',
    text: 'Tu ne peux pas poser ta dernière carte sans avoir annoncé UNO avant.',
    example: 'Il te reste une carte : appuie sur UNO !, puis pose-la.',
  },
  {
    name: 'Cumul des pénalités : Échelle',
    text: 'Un +2, un +4 ou un +5 peut se poser sur une pénalité d’un niveau égal ou plus faible (+2 < +4 < +5), et les cartes à piocher s’additionnent.',
    example:
      'Un +2 est posé sur toi : tu réponds par un +4, le suivant doit piocher 2 + 4 = 6 cartes (ou répondre par un +4 ou un +5).',
  },
  {
    name: 'Cumul des pénalités : Sans cumul',
    text: 'Une pénalité se subit tout de suite ; seul le +5 répond à un +5.',
    example: 'Un +2 sur toi : tu pioches 2 cartes et tu passes ton tour.',
  },
  {
    name: 'Cartes +2, Jokers +4 et Jokers +5 dans le paquet (×1, ×2, ×3, ×5)',
    text: 'Multiplie le nombre de ces cartes : plus il y en a, plus les pénalités tombent.',
    example: 'Avec les Jokers +5 à ×5, le paquet en compte 10 : la partie devient très cruelle.',
  },
];

/**
 * « Comment jouer » : la page pour les amis de l'hôte. Règles de base, cartes spéciales, UNO, options du salon et
 * réglages audio, lisible sur téléphone. Du texte et des cartes d'exemple : aucune dépendance au serveur.
 */
@Component({
  selector: 'app-how-to-play-page',
  imports: [CardFace, Logo, RouterLink],
  templateUrl: './how-to-play-page.html',
  styleUrl: './how-to-play-page.scss',
})
export class HowToPlayPage {
  protected readonly specials = SPECIAL_CARDS;
  protected readonly houseRules = HOUSE_RULES;
}
