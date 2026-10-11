# 0035 — Vitesse des animations réglée par l'hôte

- **Statut** : accepté
- **Date** : 2026-10-11
- **Complète** : ADR 0025 (rythme), ADR 0026 (pioche rythmée), ADR 0027 (résolution d'un effet)

## Contexte
Le lot Q avait supprimé le réglage de vitesse personnel : chaque client allant à sa vitesse, le serveur ne pouvait plus
garantir que ce que l'on montre tient dans ce qu'il attend. Les parties entre amis veulent pourtant pouvoir accélérer.

## Décision
- **Un réglage de salon, pour tout le monde** : `animationSpeed` (`normal` par défaut, `fast`), modifiable par l'hôte
  seulement, comme les autres réglages.
- **Le serveur raccourcit ses pauses** : `Timeouts::atSpeed(speed)` divise par deux `playStep`, `effectStep`,
  `drawStep`, `actionCooldown` et `forcedAction`. `Application::paceOf(room)` donne les pauses d'un salon ; elle
  alimente le budget de présentation (`actionsOpenAt`), le coup forcé et `drawStepMs` de la vue. Les délais de jeu
  (minuteur de tour, grâce et fenêtre du contre-UNO, reconnexion) ne changent pas, ni la réflexion des bots.
- **Le client raccourcit ses animations de la même façon** : `AnimationDirector.roomSpeed` (2 en rapide) divise les
  durées de `MOTION_MS` ; l'écart entre deux cartes piochées vient déjà raccourci du serveur (`drawStepMs`) et n'est
  pas divisé deux fois. Le budget serveur et les animations gardent ainsi le même rapport.

## Alternatives écartées
- **Réglage personnel dans le navigateur** : un client rapide verrait des effets finis pendant que les autres les
  regardent encore, et le serveur ne pourrait pas choisir un budget juste.
- **Facteur libre (0,5 à 3)** : une demi-vitesse et une double vitesse suffisent ; plus de choix, plus de cas à tester.

## Conséquences
- Le champ `animationSpeed` est obligatoire dans `RoomSettings` (tous les exemples du protocole sont mis à jour).
- Les E2E, qui fixent leurs propres pauses par `UNO_TEST_PACE_MS`, restent à la vitesse normale.
