# 0008 — Aléatoire déterministe et portable : moteur normalisé, tirage et mélange écrits à la main

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
La SPEC (§7.4, §17) exige qu'une graine et une liste d'actions rejouent une partie **à l'identique** : tests de
rejeu, simulation massive, E2E avec `UNO_TEST_SEED`. Le CI compile avec MSVC (bibliothèque standard de Microsoft),
GCC (libstdc++) et Clang. Or la norme ne fixe que la sortie des **moteurs** (`std::mt19937_64`, [rand.predef] :
la 10 000e valeur d'un moteur construit par défaut vaut 9981545732273789042). Les **distributions**
(`std::uniform_int_distribution`) et `std::shuffle` ont un algorithme laissé à l'implémentation : la même graine
donnerait une partie différente sous Windows et sous Linux.

## Décision
- `SeededRandomSource` (tests et simulation uniquement, bibliothèque `uno_testing`) utilise `std::mt19937_64`, dont
  la sortie est garantie par la norme, et un **échantillonnage par rejet écrit à la main** pour
  `uniform(upperExclusive)` : les tirages inférieurs à `2^64 mod n` sont rejetés, puis on renvoie `tirage % n`.
  C'est la méthode de libsodium : pas de biais de modulo.
- `uno::core::shuffle(std::span<T>, RandomSource&)` est un **Fisher-Yates écrit à la main** qui ne s'appuie que sur
  `RandomSource::uniform`. Il sert aussi en production avec `CryptoRandomSource`.
- `uniform(0)` et `uniform(1)` renvoient 0, comme `randombytes_uniform` de libsodium : les deux implémentations
  ont le même contrat.
- Deux tests verrouillent la décision : l'un vérifie la valeur de référence de la norme (l'hypothèse), l'autre des
  valeurs de référence de notre tirage (l'implémentation), et le CI les exécute sur les trois compilateurs.

## Alternatives écartées
- **`std::uniform_int_distribution` / `std::shuffle`** : portables à la compilation, mais pas dans leurs résultats.
- **Bibliothèque externe (PCG, Boost.Random)** : une dépendance de plus dans `uno_core`, qui doit rester sans
  dépendance, pour une dizaine de lignes.
- **Tirage par simple modulo (`moteur() % n`)** : biaisé. Le biais est infime avec 64 bits, mais c'est une mauvaise
  habitude à enseigner.

## Conséquences
- Une graine donne la même partie sur toutes les plateformes : un bug observé sous Linux se rejoue sous Windows.
- Changer l'algorithme de tirage ou de mélange change toutes les parties rejouées : les valeurs de référence du test
  doivent alors être mises à jour sciemment, dans une PR dédiée.
