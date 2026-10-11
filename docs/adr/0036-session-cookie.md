# 0036 — La session dans un cookie HttpOnly, un seul onglet actif, et des plafonds

- **Statut** : accepté
- **Date** : 2026-10-11
- **Remplace** : le jeton de session dans `sessionStorage` et dans les messages (SPEC §9.3, ADR 0014)
- **Complète** : ADR 0016 (limites de débit), ADR 0033 (tunnel)

## Contexte
Le jeton de session vivait dans `sessionStorage`, lisible par n'importe quel script de la page, et il voyageait dans
`session.hello` et `session.welcome`. Fermer l'onglet le perdait : le joueur ne retrouvait plus sa place. Rien
n'empêchait non plus un visiteur de monopoliser le serveur.

## Décision
- **Le jeton est dans un cookie** `uno_session` : `HttpOnly` (invisible aux scripts), `Secure`, `SameSite=Strict`,
  `Path=/`, `Max-Age=86400`. Le serveur le pose dans la réponse **101** du handshake WebSocket (une réponse `Set-Cookie`
  est traitée par le navigateur comme toute autre). Sans cookie valide (22 caractères URL-safe), le serveur fabrique un
  jeton avec le hasard cryptographique (`CryptoRandomSource`) ; le jeton arrive à l'application par
  `ConnectionHandler::onConnected(connexion, jeton)`. Il quitte donc le protocole : `session.hello` ne porte plus que
  la version du client et `session.welcome` plus que `playerId` et `resumedRoomCode`.
- **Fermer l'onglet, puis revenir** : le navigateur renvoie le cookie, `session.hello` retrouve la session et son salon
  (la grâce de reconnexion s'applique comme avant). Localement, `Secure` est accepté par les navigateurs pour
  `localhost` et `127.0.0.1` ; à travers le tunnel (HTTPS, Caddy relaie l'en-tête) aussi.
- **Un seul onglet actif par navigateur** : si la session est déjà tenue par une connexion, la nouvelle reçoit
  `session.busy` et rien n'est modifié. `session.takeOver` la prend et ferme l'ancienne (code 4000, qui ne se reconnecte pas :
  elle affiche « Partie ouverte dans un autre onglet », « Jouer ici »). Conséquence assumée : pour tester à deux sur
  une machine, deux navigateurs ou une fenêtre privée (`docs/MODE-EMPLOI.md`, README).
- **Anti-abus** : un salon actif par session (déjà vrai : `ALREADY_IN_ROOM`) ; 20 connexions par adresse et 1 000 au total
  (503 au handshake) ; 300 salons et 2 000 sessions (`SERVER_FULL`, `Limits` de l'application) ; salons vides détruits ;
  l'adresse du client, pour les limites par adresse, n'est lue dans `X-Forwarded-For` que si `UNO_TRUSTED_PROXY` est vrai
  **et** que le pair est la machine elle-même (Caddy local) : sinon l'en-tête est une falsification possible.

## Alternatives écartées
- **Garder `sessionStorage` ou passer à `localStorage`** : lisible par un script (XSS) et perdu à la fermeture (le premier).
- **Cookie posé par une requête HTTP dédiée avant le WebSocket** : une route de plus à protéger, pour le même résultat.
- **Reprise automatique de l'autre onglet (le plus récent gagne)** : deux onglets se voleraient la session en boucle ;
  mieux vaut demander.

## Conséquences
- Le client n'a plus de `SESSION_EXPIRED` à gérer : un cookie inconnu ouvre simplement une nouvelle session.
- Les tests du serveur ouvrent leurs connexions avec un jeton (le transport de test l'écrit dans l'en-tête `Cookie` et lit
  `Set-Cookie`) ; les graines des E2E sont recalculées (le jeton ne consomme plus le hasard de la partie).
