# 0014 — Couche réseau : modèle de messages dans `app`, limites du transport, client de test sans dépendance

- **Statut** : accepté
- **Date** : 2026-10-01

## Contexte
La SPEC §8.1 place les DTO C++ du protocole dans `server/net/protocol/`, et §6 impose `net → app → core`. Or la
couche `app` (salons, sessions) doit lire les requêtes et produire les réponses : si les DTO vivaient dans `net`,
`app` dépendrait de `net`. §9.4 demande 4 Kio par message ; §9.2 un seul thread. Enfin les tests d'intégration
(étape 2.6) ont besoin d'un client WebSocket.

## Décision
- **Le modèle de messages est dans `uno_app`** (`client_message.hpp` : `request::Envelope` et ses 19 corps ;
  `server_message.hpp` : `response::Message`), en types forts et sans JSON. `uno_net` ne contient que le **codec**
  (JSON ↔ ces types, validation stricte, nlohmann-json en dépendance privée) : `app` et `core` ignorent le format
  de fil. C'est un écart à §8.1 (emplacement des DTO) qui garde la règle de dépendance.
- **Décodage strict sans exception** : version (`UNSUPPORTED_VERSION`), puis type (`UNKNOWN_TYPE`), puis forme
  (`MALFORMED_MESSAGE`) ; champ manquant, en trop ou mal typé = rejet ; un nombre à virgule est refusé même s'il est
  entier (`1.0`). Le message d'erreur reprend `id` en `replyTo` quand il est lisible. Les noms de fil de chaque
  énumération sont dans une seule table (`wire_names.hpp`), utilisée dans les deux sens.
- **Limites de taille** : un message fait au plus 4 Kio (`kMaxMessageBytes`) ; entre 4 et 8 Kio le serveur ferme
  avec le code 1009 et la raison `MESSAGE_TOO_LARGE` (comme l'annonce le schéma) ; au-delà de 8 Kio, c'est
  uWebSockets qui coupe la connexion (`maxPayloadLength`), pour ne jamais tamponner plus. Trames binaires : 1003.
  Dix messages mal formés sur une connexion : 1008. Trop de retard d'écriture (64 Kio) : connexion fermée.
- **`Origin`** : liste exacte (`UNO_ALLOWED_ORIGINS`, sans joker), comparée sans casse ; une requête **sans**
  `Origin` est refusée (un navigateur l'envoie toujours). Par défaut : les deux origines du serveur de dev Angular.
- **Port exclusif** (`LIBUS_LISTEN_EXCLUSIVE_PORT`) : sous Windows, deux processus ne partagent pas silencieusement
  le même port.
- **Client de test sans dépendance vcpkg** : `tests/support/test_client.*` est un client HTTP/WebSocket minimal sur
  sockets bruts (poignée de main, trames masquées de tout opcode, lecture des trames et du code de fermeture),
  une centaine de lignes utiles, Winsock ou POSIX. Il sait envoyer ce qu'une bibliothèque refuserait d'envoyer
  (trame de 1 Mio, binaire, JSON invalide), ce qui est précisément ce qu'on teste. Aucune dépendance de plus à
  figer dans la baseline, auditer ou compiler sur les trois plateformes du CI.

## Conséquences
- Chaque nouveau message du protocole se code dans trois endroits : le schéma, `app/…_message.hpp`, le codec ; le
  test de contrat (`protocol_contract_test.cpp`) relit tous les exemples de `protocol/examples` et casse si l'un
  d'eux diverge.
- Le client de test est du code de test à maintenir ; en échange, aucune API tierce ne dicte ce qu'on peut envoyer.

## Alternatives écartées
- **DTO dans `net`, `app` qui en dépend** : casse `net → app → core`.
- **Un seul niveau de limite (4 Kio, géré par uWebSockets)** : le client ne saurait pas pourquoi il est coupé.
- **IXWebSocket ou Boost.Beast pour les tests** : une dépendance transitive de plus (zlib, OpenSSL ou Boost) et
  des clients qui normalisent les trames, donc incapables de reproduire les cas hostiles.
