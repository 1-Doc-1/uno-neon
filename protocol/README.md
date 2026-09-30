# Protocole client ↔ serveur (v1)

Source de vérité du protocole WebSocket (voir `docs/SPEC.md` §8 et les écarts arbitrés dans
`docs/adr/0007-protocol-v1-deviations.md`).

## Schémas (`schema/`, JSON Schema 2020-12)

| Fichier                      | Contenu                                                           |
| ---------------------------- | ----------------------------------------------------------------- |
| `protocol.schema.json`       | point d'entrée : un message client ou un message serveur          |
| `client-message.schema.json` | les intentions envoyées par le client (discriminées par `type`)   |
| `server-message.schema.json` | `ack`, `error` et les messages poussés par le serveur             |
| `room-view.schema.json`      | état d'un salon (lobby)                                           |
| `player-view.schema.json`    | vue d'une partie pour **un** joueur (seule donnée de jeu envoyée) |
| `client-event.schema.json`   | événements à animer (discriminés par `kind`)                      |
| `common.schema.json`         | types partagés (cartes, identifiants, réglages, codes d'erreur)   |

Tous les objets sont fermés (`additionalProperties: false`). Le mot-clé `tsType` est une extension de
`json-schema-to-typescript`, ignorée par la validation.

## Exemples (`examples/`)

- `valid/` : au moins un message valide par type (`client.<type>[.variante].json`, `server.<type>[.variante].json`),
  et chaque type d'événement apparaît dans au moins un `server.game.update.*`.
- `invalid/` : messages client que le serveur doit rejeter. Le nom commence par le code d'erreur attendu :
  `UNKNOWN_TYPE.reveal-hands-cheat.json`. Un fichier qui n'est même pas du JSON porte l'extension `.txt`.

Le serveur vérifie `v` (`UNSUPPORTED_VERSION`), puis `type` (`UNKNOWN_TYPE`), puis le reste du schéma
(`MALFORMED_MESSAGE`).

## Faire évoluer le protocole

1. Modifier le schéma et ajouter/adapter un exemple.
2. `npm run protocol:gen` (depuis `client/`) pour régénérer `client/src/app/protocol/generated/protocol.ts`.
3. `npm test` (depuis `client/`) : le test de contrat valide tous les exemples.
4. Adapter les DTO C++ (`server/net/protocol/`, à partir de la phase 2) et leurs tests de contrat.
