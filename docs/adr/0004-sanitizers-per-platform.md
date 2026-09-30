# 0004 — Sanitizers : ASan + UBSan sous Linux, ASan seul sous MSVC

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
Les sanitizers détectent à l'exécution les erreurs mémoire (AddressSanitizer) et les comportements indéfinis
(UndefinedBehaviorSanitizer) que le compilateur ne voit pas. La SPEC demande un preset `debug-asan`. Or MSVC ne
fournit qu'ASan, et les bibliothèques vcpkg ne sont pas instrumentées.

## Décision
- Un seul preset `debug-asan`, avec l'option CMake `UNO_SANITIZERS=ON` ; `cmake/Sanitizers.cmake` choisit les
  options selon le compilateur et les applique à **toutes** nos cibles (tests compris).
- GCC / Clang : `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer` (toute erreur
  arrête le test).
- MSVC : `/fsanitize=address`, sans vérifications `/RTC` (incompatibles), sans liaison incrémentale, et avec
  `_DISABLE_STL_ANNOTATION` (sinon erreur `LNK2038 annotate_optional/annotate_vector` à l'édition de liens avec
  les bibliothèques vcpkg non instrumentées).
- Le CI Linux (`server-linux`, GCC et Clang, preset `debug-asan`) est la **référence** ; le preset sous Windows est
  un confort pour déboguer localement.

## Alternatives écartées
- **Deux presets distincts (`debug-asan-linux`, `debug-asan-windows`)** : même intention, deux noms à retenir.
- **Instrumenter aussi les dépendances vcpkg** (triplet personnalisé) : build beaucoup plus long pour un gain faible,
  nos bugs seront dans notre code.

## Conséquences
- Un comportement indéfini peut passer inaperçu sous Windows et n'être détecté qu'en CI : il faut lire les échecs
  du job Linux même quand « ça marche sur mon PC ».
