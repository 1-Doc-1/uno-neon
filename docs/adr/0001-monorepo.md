# 0001 — Un seul dépôt pour le serveur, le client et le protocole

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
UNO Néon se compose d'un serveur C++, d'un client Angular et d'un contrat (le protocole WebSocket) qui les relie.
Le contrat évolue en même temps que les deux côtés : un changement de message touche le schéma, les types TypeScript
générés, les DTO C++ et leurs tests. L'équipe est petite (étudiants) et travaille par étapes courtes relues en PR.

## Décision
Un dépôt unique (monorepo) :

```
protocol/   schémas JSON (source de vérité) et exemples
server/     C++23 (uno_core, uno_app, uno_net, uno_server)
client/     Angular
e2e/        Playwright (phase 4)
deploy/     Docker, Caddy (phase 6)
docs/       SPEC, PROGRESS, ADR
```

Chaque sous-projet garde son propre outillage (CMake/vcpkg d'un côté, npm de l'autre) : pas d'outil de monorepo
(Nx, Bazel…) par-dessus.

## Alternatives écartées
- **Un dépôt par composant** : une modification du protocole demanderait plusieurs PR synchronisées et des numéros
  de version entre dépôts ; trop de friction pour une petite équipe.
- **Outil de monorepo (Nx, Bazel)** : puissant mais coûteux à apprendre et à configurer pour deux sous-projets.

## Conséquences
- Une seule PR peut modifier le schéma, les types générés et le code des deux côtés, et le CI vérifie le tout.
- Le CI doit exécuter des jobs indépendants par sous-projet (serveur, client) pour rester rapide.
- Les chemins relatifs entre sous-projets (ex. `client/` qui lit `../protocol/`) font partie du contrat du dépôt.
