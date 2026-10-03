# 0020 — L'horloge d'un tour n'est réarmée que quand le tour change

- **Statut** : accepté (précise l'[ADR 0016](0016-time-and-departures.md))
- **Date** : 2026-10-03

## Contexte
L'ADR 0016 réarmait le minuteur de tour à **chaque** diffusion de la partie, avec le `stateVersion` pour garde. Depuis
l'ADR 0018 certaines diffusions n'ont rien à voir avec le tour : l'échéance d'une fenêtre de contre-UNO en envoie une
15 s après son ouverture, un contre-UNO ou une annonce aussi. Le joueur dont c'était le tour repartait alors pour 30 s
(un test de l'ADR 0019 l'a révélé : un tour qui devait expirer à 30 s expirait à 45 s).

## Décision
Le minuteur de tour est identifié par une **clé de tour** (`TurnKey` : numéro de manche, joueur courant, phase du tour).
- Une diffusion qui laisse la clé inchangée ne touche ni au minuteur ni à `turnDeadline`.
- Une clé différente (autre joueur, autre phase : pioche puis décision, manche suivante) arme une nouvelle horloge
  et incrémente `turnEpoch`.
- Le callback porte le `turnEpoch` pour lequel il a été armé : un callback d'une horloge remplacée ne fait rien (il ne
  dépend plus du `stateVersion`).
- À l'expiration, la clé est effacée : même si l'action automatique était refusée, la diffusion suivante arme une horloge.

Les autres timers (coup forcé de la pioche guidée, manche suivante) gardent le `stateVersion` : ils doivent suivre
chaque changement d'état.

## Conséquences
- Un joueur qui n'est pas le joueur courant ne peut plus prolonger un tour en contrant ou en annonçant.
- Un joueur qui pioche puis décide reçoit toujours une horloge neuve pour sa décision (comportement inchangé).
