# 0016 — Le temps et les départs : timers, retrait d'un joueur, limitation de débit, options de test

- **Statut** : accepté
- **Date** : 2026-10-01

## Contexte
L'étape 2.5 ajoute tout ce que le silence ou le départ d'un joueur déclenche (SPEC §4, §5, §9.3, §9.4) : minuteur de
tour, délai de grâce de reconnexion, expiration des salons et des sessions, retrait d'un joueur d'une manche, limites
de débit. La SPEC dit *quoi* ; plusieurs points de *comment* n'y sont pas, et le moteur ne savait pas retirer un joueur.

## Décision
- **Port `Scheduler`** (`uno_app`) : `schedule(délai, callback) → TimerHandle`, `cancel`. `UwsScheduler` (`uno_net`)
  en production, sur les timers de la boucle uWebSockets, avec `fallthrough` (un timer ne retient jamais la boucle) ;
  `ManualScheduler` (`uno_testing`) en test : le temps avance à la main, chaque callback voit l'horloge à son
  échéance. À l'arrêt du serveur, `onStop` annule tous les timers sur le thread de la boucle : libuv refuse de
  fermer une boucle dont des handles sont ouverts.
- **Un timer périmé ne fait rien** : chaque changement d'état d'une partie réarme les timers (`armGameTimers`) et un
  timer de tour ou de manche suivante porte le `stateVersion` pour lequel il a été armé. Une action jouée à temps
  annule le timer ; si l'annulation échappait, la version ne correspondrait plus.
- **Durées** (`Timeouts`, injectables) : grâce 60 s ; salon en lobby inactif 15 min ; partie finie 5 min ; session sans
  connexion 10 min ; manche suivante lancée d'office 30 s après la fin d'une manche (la SPEC cite
  `nextRoundDeadline` sans durée : choix à confirmer).
- **Action automatique à l'expiration du tour** : pénalité en attente → l'accepter ; couleur à choisir → couleur
  tirée au hasard ; carte piochée à décider → passer ; sinon piocher une carte puis passer, même si elle est jouable.
  Elle passe par `Match::apply`, comme l'action d'un joueur : mêmes règles, mêmes événements, et la fenêtre UNO se
  ferme (revue de la phase 1, point 3).
- **Retirer un joueur** (`Round::removePlayer`, `Match::removePlayer`) : ses cartes passent sous la pioche ; si c'était
  son tour, le tour passe au suivant dans le sens du jeu ; un +4 en attente dont il est la cible ou le poseur est
  annulé ; une première couleur à choisir est tirée au hasard ; le donneur retiré est remplacé par le siège précédent.
  À deux joueurs, la manche ne peut plus continuer : le **match se termine par forfait** (le restant gagne,
  `MatchEnded`), et la vue nomme encore le siège parti (`formerNicknames`). Les scores des autres sont conservés.
  La simulation massive retire des joueurs au hasard et vérifie toujours les mêmes invariants.
- **Quitter, exclure, ne pas revenir** passent tous par `removeFromRoom` : un seul chemin pour les trois, qui passe
  l'hôte au joueur connecté suivant (`hostChanged` en partie), diffuse, et ferme le salon quand il est vide.
- **Limites de débit dans `uno_net`**, avant d'atteindre l'application : seau à jetons par connexion (capacité 20,
  10/s) appliqué à chaque trame **avant** de la lire (un flot de déchets coûte autant qu'un flot de messages valides ;
  un refus compte comme une erreur de format, 10 refus = fermeture 1008), fenêtre glissante par adresse pour
  `room.create` (5/min) et `room.join` (20/min). L'adresse est celle de la connexion ; `X-Forwarded-For`
  (dernière entrée, ajoutée par notre proxy) n'est lu que si `UNO_TRUSTED_PROXY` est vrai.
- **`UNO_TEST_SEED` n'existe qu'à la compilation** : l'option CMake `UNO_ENABLE_TEST_HOOKS` (désactivée par défaut)
  lie `uno_testing` à `uno_server` et définit la macro ; sans elle, ni la lecture de la variable ni son nom ne sont
  dans le binaire, ce que le CI vérifie (`grep` du binaire de production).

## Conséquences
- Un rejet par limite de débit avant lecture ne peut pas porter le `replyTo` du message : le client ne l'attribue pas
  à une requête précise. Aucun humain n'approche 10 messages par seconde.
- Les tests d'intégration (bots qui jouent aussi vite que le serveur répond) relèvent les limites de leur propre
  déploiement ; les limites réelles sont testées à part avec de petites valeurs.

## Alternatives écartées
- **Retirer le joueur du `Round` en le remplaçant par un bot passif** : le jeu ne finirait jamais si tous partent.
- **Rejeter `room.leave` en partie** : un joueur doit pouvoir quitter, et le délai de grâce a besoin du même retrait.
- **Compter les erreurs de débit à part des erreurs de format** : deux compteurs pour le même but (ne pas répondre
  indéfiniment à un client abusif).
