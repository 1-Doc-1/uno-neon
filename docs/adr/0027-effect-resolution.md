# 0027 — Résolution d'un effet : le serveur dit quand le tour s'ouvre (`actionsOpenAt`)

- **Statut** : accepté
- **Date** : 2026-10-04
- **Complète** : ADR 0026 (la pioche rythmée devient un cas particulier), ADR 0020 (clé de tour)

## Contexte
Après un +2, un +4 ou un Passe, le joueur qui l'avait infligé pouvait rejouer avant que les cartes de pénalité ne
soient arrivées chez l'autre : à deux joueurs, le Passe ou le +2 lui rend la main aussitôt, et l'interface le laissait
cliquer pendant l'animation. Griser l'interface ne suffit pas (toute entrée réseau est hostile) : le serveur doit refuser.

## Décision
- **Un budget de présentation par action** (`Application::presentationBudgetOf`), calculé sur les événements du lot et
  l'état d'après : une carte posée (`playStep`, 1,1 s), chaque effet spécial (`effectStep`, 1,2 s : Passe, inversion,
  choix de couleur, verdict d'une contestation, contre-UNO, et le « +2 » / « +4 » d'une carte posée), chaque carte
  piochée (`drawStep`, 1 s). Les trois durées sont des `Timeouts` injectables ; les cartes piochées sont donc un cas
  particulier du même calcul (ADR 0026 : « N × drawStep »), il n'y a plus qu'une source.
- **`actionsOpenAt`** (heure serveur, dans chaque `PlayerView`) : `max(ancienne valeur, maintenant + budget)`. Il ne
  recule jamais : une annonce de UNO pendant la résolution (budget nul) ne l'avance pas.
- **Refus** : une action de tour (`playCard`, `drawCard`, `pass`, `chooseColor`, `respondPenalty`) reçue du joueur dont
  c'est le tour avant `actionsOpenAt` reçoit `EFFECT_IN_PROGRESS`, sans rien changer. Un joueur qui n'a pas la main
  reçoit toujours `NOT_YOUR_TURN`. **Annoncer UNO et le contre-UNO restent permis** (leurs fenêtres sont inchangées).
- **Les minuteurs partent de `actionsOpenAt`** : l'horloge d'un tour (`turnDeadline = actionsOpenAt + durée du tour`) et
  le coup forcé (`actionsOpenAt + forcedAction`). Les actions automatiques (minuteur de tour, coup forcé) passent par
  `Match::apply`, pas par le refus : elles tombent après l'ouverture. La clé de tour (ADR 0020) compte désormais les
  changements de tour (`TurnChanged`) : un joueur qui rejoue juste après son propre +2 à deux joueurs a une nouvelle
  horloge, qui part elle aussi de l'ouverture.
- **Client** : la main est neutre, le paquet muet et la fenêtre de réponse à une pénalité retenue tant que l'heure du
  serveur (corrigée de l'écart d'horloge) n'a pas atteint `actionsOpenAt` ; l'ouverture est programmée à l'heure dite.
  `drawStepMs` reste envoyé : le client rythme l'arrivée des cartes sur lui.
- **Crochet de test unique** : `UNO_TEST_PACE_MS` (binaire de test seulement) fixe `playStep`, `effectStep` et
  `drawStep` ; il remplace `UNO_TEST_DRAW_STEP_MS`. Les E2E l'utilisent à 40 ms par défaut.

## Alternatives écartées
- **Le client dit quand il a fini d'animer** : un client malveillant ou lent ne doit pas décider du rythme de la table.
- **Suspendre le jeu pendant l'animation** : un état de plus ; une échéance dans la vue suffit, comme pour la pioche.
- **Refuser aussi UNO / contre-UNO** : le contre-UNO a sa propre fenêtre (ADR 0018) et doit rester possible.

## Conséquences
- Le budget est une **estimation prudente** des animations normales du client : un client en
  mouvement réduit finit avant l'ouverture, un client très en retard (onglet en arrière-plan) la voit passer.
- Chaque coup rend la main plus tard (une carte posée : 1,1 s), y compris pour une pose simple : c'est le prix d'une
  table où l'on voit ce qui vient de se passer.
- Les tests d'application jouent sans délai (`withoutPresentationDelay`) ; ceux du rythme utilisent les vraies durées.

## Amendement (lot Q) : un délai fixe après chaque action, un budget qui couvre chaque carte
- **Constat** : en partie réelle, le joueur suivant jouait parfois avant la fin de l'animation d'un Passe. Le budget est une estimation ; une estimation trop juste (latence, onglet lent) ne suffit pas.
- **`actionCooldown` (1500 ms, injectable)** : après CHAQUE action de tour (pose, pioche, passe, choix de couleur, réponse à une pénalité), `actionsOpenAt = max(ancienne valeur, maintenant + max(budget de présentation, actionCooldown))`. Identique pour tous les joueurs. Annoncer UNO et contrer n'ouvrent pas ce délai. Le crochet de test `UNO_TEST_PACE_MS` le fixe aussi.
- **`effectStep` passe de 1,2 s à 1,5 s** : le client montre l'Inversion pendant 1,4 s (plus la carte, 1,08 s) ; 1,2 s ne la couvrait pas. Le test `presentation-budget.spec.ts` (client) vérifie, pour Passe, Inversion, +2, +4 et Joker, que la durée réelle de l'animation tient dans le budget, et `application_presentation_test.cpp` (serveur) verrouille les sommes du budget par carte. L'éclat de tour (0,5 s), qui accompagne l'ouverture du tour, n'est pas compté.
- **Plus de vitesse réglable côté client** : le réglage « Normale / Rapide » est supprimé (le serveur dicte le rythme).
