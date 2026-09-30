# 0005 — JSON Schema 2020-12 comme source de vérité du protocole

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
Le client (TypeScript) et le serveur (C++) échangent des messages JSON sur `/ws`. Si chaque côté décrit les messages
à sa façon, ils divergent sans que personne ne s'en aperçoive avant une partie réelle. Le contrat doit aussi exister
**avant** le code, pour que l'équipe UI (données simulées) et l'équipe serveur avancent en parallèle.

## Décision
- `protocol/schema/*.schema.json` (JSON Schema 2020-12) est l'unique description du protocole :
  `common` (types partagés), `client-message`, `server-message`, `room-view`, `player-view`, `client-event`,
  et `protocol` (point d'entrée).
- Toujours `additionalProperties: false` : un champ inconnu est une erreur, des deux côtés.
- **TypeScript** : types générés par `json-schema-to-typescript` (`npm run protocol:gen`) dans
  `client/src/app/protocol/generated/protocol.ts`, jamais modifiés à la main. Le CI régénère et échoue si le fichier
  versionné diffère.
- **C++** : DTO écrits à la main (phase 2), vérifiés par des tests de contrat sur les mêmes exemples.
- **Exemples** : `protocol/examples/valid/` (au moins un par type de message, et chaque type d'événement utilisé au
  moins une fois) et `protocol/examples/invalid/` (nom de fichier = code d'erreur attendu). Un test Vitest (avec
  `ajv`) vérifie tout cela à chaque PR.
- Le serveur vérifie `v` puis `type` avant le reste, pour répondre `UNSUPPORTED_VERSION` ou `UNKNOWN_TYPE` ; toute
  autre violation du schéma est `MALFORMED_MESSAGE`. Les règles métier (pseudo, réglages cohérents) ont leurs propres
  codes (`NICKNAME_INVALID`, `INVALID_SETTINGS`) et ne sont pas dans le schéma.

## Alternatives écartées
- **Types TypeScript écrits à la main** : aucun lien vérifiable avec le C++.
- **Protobuf / FlatBuffers** : binaire, illisible dans l'onglet Réseau du navigateur, outillage plus lourd.
- **Générer aussi le C++ depuis le schéma** (quicktype…) : code généré peu idiomatique, qui ne respecterait ni nos
  types forts (`PlayerId`, `CardId`) ni `std::variant` ; des DTO manuscrits vérifiés par tests de contrat sont plus
  lisibles pour des étudiants.
- **OpenAPI / AsyncAPI** : pensés pour HTTP ou des brokers de messages, surdimensionnés ici.

## Conséquences
- Toute évolution du protocole commence par le schéma et un exemple, puis `npm run protocol:gen`.
- `json-schema-to-typescript` ne connaît pas tout JSON Schema 2020-12 : on reste sur un sous-ensemble simple
  (`$ref`, `$defs`, `oneOf`, `anyOf`, `const`, `enum`). L'extension `tsType` sert uniquement à typer le payload vide
  en `Record<string, never>`.
