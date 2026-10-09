# 0026 — Pioche rythmée : le serveur décide du rythme, les clients le suivent

- **Statut** : accepté (le calcul du délai est généralisé par l'ADR 0027)
- **Date** : 2026-10-04

## Contexte
Retour de partie : 150 ms par carte piochée, c'est trop rapide pour voir son tas grandir. Allonger seulement
l'animation du client désynchroniserait le jeu : 12 cartes à 1 s font 12 s d'animation, pendant lesquelles le minuteur
de tour tourne et où le coup forcé du serveur (pose automatique de la dernière carte piochée, ADR 0017) partirait
trop tôt. Le serveur ne sait pas ce que le client affiche, il doit donc dicter le rythme, et le client le suivre.

## Décision
- **Un délai injectable** `Timeouts::drawStep` (1000 ms par défaut ; `UNO_TEST_PACE_MS` dans un binaire de test depuis l'ADR 0027).
- **Après une action qui fait piocher N cartes** (pioche, pénalité +2/+4, contestation, contre-UNO), le serveur compte
  les cartes des événements du lot (`drawPauseOf`) : le prochain coup forcé attend `N × drawStep + forcedAction`, et
  l'horloge d'un tour qui commence là démarre après ce temps (`turnDeadline` prolongé d'autant). Ce temps est une
  prolongation, non une suspension : aucun état de « pause » à gérer, le minuteur est simplement armé plus tard.
- **`drawStepMs` est envoyé au client** dans chaque vue (`PlayerView.drawStepMs`, schéma `player-view`). Le client ne
  connaît aucune constante de rythme de pioche : `ui/motion.ts` ne garde que la durée du vol d'une carte.
- **Côté client**, les événements `cardsDrawn` consécutifs d'un même joueur (un par carte) forment **une seule étape** :
  elle ne compte qu'une fois pour la règle de rattrapage de la file, n'est jamais accélérée, et n'est jamais abandonnée
  pour cause de retard (le serveur attend sa fin). Chaque carte part du paquet à `i × drawStepMs`. À sa partie, le
  compteur du joueur (siège adverse, pastille, éventail) et la carte de ma main n'apparaissent qu'à l'arrivée
  (`unarrived`), et le compteur du paquet ne baisse qu'au départ (`unlaunched`).
- **Vitesse « Rapide »** : seul le vol de chaque carte raccourcit ; l'écart entre deux cartes reste celui du serveur.
- **Mouvement réduit** : même rythme, le vol devient un fondu (les compteurs, eux, ne bougent pas : ils comptent).

## Alternatives écartées
- **Suspendre le minuteur de tour** pendant la pioche : un état de plus (suspendu/reprise), et le minuteur reprendrait
  avec le reliquat d'avant. Armer plus tard donne le même effet pour le joueur.
- **Un rythme décidé par le client** : se désynchronise du coup forcé et du minuteur (cause du problème).
- **Rythme réglable par salon** : pas demandé ; une valeur serveur suffit.

## Conséquences
- Une pioche de 12 cartes immobilise le jeu 12 s pour tout le monde, voulu : c'est le temps de la regarder.
- Les tests de timing de l'application qui attendaient exactement 1,2 s après une pioche ajoutent désormais le temps
  des cartes ; le test `[pace]` fixe les bornes exactes.
- Les E2E raccourcissent le rythme (`UNO_TEST_PACE_MS`, 40 ms par défaut dans la pile de test).
