# 0003 — vcpkg en mode manifest avec baseline figée

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
Le serveur dépend de uWebSockets, nlohmann-json, libsodium, spdlog et Catch2. Il faut que chaque poste et le CI
utilisent exactement les mêmes versions, sans installation manuelle, sous Windows comme sous Linux.

## Décision
- `server/vcpkg.json` (mode manifest) liste les dépendances ; `cmake --preset …` les installe automatiquement dans
  le dossier de build.
- `builtin-baseline` figée sur un commit précis du registre vcpkg : les versions ne changent que par une PR qui
  modifie cette ligne.
- Chaque membre utilise son propre clone de vcpkg (`VCPKG_ROOT=C:\dev\vcpkg`), pas celui intégré à Visual Studio.
- Les cinq dépendances sont déclarées dès la phase 0 (même celles utilisées en phase 2) pour détecter au plus tôt
  un problème de compilation sous Windows ou Linux.
- uWebSockets est pris **sans** SSL : le TLS est terminé par Caddy (phase 6), ce qui évite OpenSSL.

## Alternatives écartées
- **Mode classique (`vcpkg install` global)** : versions différentes d'un poste à l'autre, rien de versionné.
- **Conan** : aussi bon, mais une deuxième chaîne (Python) à installer et à apprendre.
- **FetchContent / sous-modules git** : recompilation de toutes les dépendances à chaque build propre et pas de
  cache binaire partagé.

## Conséquences
- Le premier `cmake --preset` est long (compilation des dépendances), les suivants utilisent le cache binaire
  vcpkg. Le CI met ce cache en `actions/cache`.
- Mettre à jour une dépendance = changer la baseline, reconstruire, relancer tous les tests.
