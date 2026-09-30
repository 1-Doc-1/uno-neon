# 0002 — CMake presets et générateur Ninja

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
Le serveur doit se construire à l'identique sur les postes Windows (MSVC) et dans le CI Linux (GCC, Clang). Sans
convention, chacun tape sa propre ligne `cmake -G … -D…` et les builds divergent. L'éditeur (clangd) a besoin d'un
fichier `compile_commands.json` pour comprendre le code.

## Décision
- `server/CMakePresets.json` versionné, avec trois presets : `dev` (Debug), `debug-asan` (Debug + sanitizers),
  `release`. Chaque preset a son build preset et son test preset de même nom.
- Générateur **Ninja** pour tous les presets, dossier de build `server/build/<preset>`.
- `CMAKE_EXPORT_COMPILE_COMMANDS=ON` dans le preset de base.
- La chaîne d'outils vcpkg est déclarée dans le preset (`$env{VCPKG_ROOT}`) : personne n'a à la passer à la main.

Trois commandes suffisent partout : `cmake --preset dev`, `cmake --build --preset dev`, `ctest --preset dev`.

## Alternatives écartées
- **Générateur Visual Studio (.sln)** : ne produit pas `compile_commands.json`, et n'existe pas sous Linux.
- **Makefiles** : plus lents que Ninja, et absents des postes Windows.
- **Scripts maison (`build.ps1`, `build.sh`)** : à maintenir en double, alors que les presets sont lus nativement
  par CMake, CLion, VS Code et Visual Studio.

## Conséquences
- Sous Windows, il faut un terminal où l'environnement MSVC x64 est chargé (`dev64`), car Ninja n'embarque pas de
  détection du compilateur.
- Les réglages personnels vont dans `CMakeUserPresets.json`, ignoré par git.
