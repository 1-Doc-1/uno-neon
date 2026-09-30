# 0009 — Modélisation des cartes : `std::optional<Color>` et fonction libre `createStandardDeck`

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
En implémentant l'étape 1.1, deux passages de la SPEC posaient problème :
- le glossaire (§2) prévoit une valeur `Color::None` « pour un Joker non posé », alors que §7.1 modélise la carte
  avec `std::optional<Color> color` ;
- §7.3 nomme la factory `DeckFactory::createStandardDeck(RandomSource&)`, c'est-à-dire une classe qui ne
  contiendrait qu'une fonction statique.

## Décision
- `enum class Color { Red, Yellow, Green, Blue }` **sans** `None`. L'absence de couleur d'un Joker s'écrit
  `std::optional<Color>` vide, comme dans le protocole (`color: null`) et pour `currentColor: Color | null` (ADR 0007).
- La factory est une **fonction libre** `uno::core::createStandardDeck(RandomSource&)` dans `deck.hpp`. Elle
  renvoie les 108 cartes dans un ordre canonique fixe, avec des identifiants 0…107 attribués selon une permutation
  aléatoire. Le mélange de la pioche est une autre responsabilité (`shuffle`).
- La SPEC est corrigée sur ces deux lignes, avec un renvoi vers cette ADR.

## Alternatives écartées
- **`Color::None`** : une « couleur » qui n'en est pas une. Chaque `switch` sur `Color` devrait la traiter, et
  `currentColor == None` ferait double emploi avec l'absence de valeur du protocole.
- **Classe `DeckFactory` à méthode statique** : en C++, une classe sans état qui ne contient que des fonctions
  statiques joue le rôle d'un espace de noms. Le pattern Factory (un point unique qui construit) est le même avec
  une fonction libre, qui est plus simple.
- **Constructeurs dédiés dans `Card` (`Card::wild`, `Card::colored`)** pour rendre `Card{Red, Wild}`
  irreprésentable : écarté pour l'instant. La factory est la seule source de cartes et un test vérifie l'invariant
  « couleur ⇔ pas un Joker ».

## Conséquences
- Le code qui lit une couleur doit traiter l'absence explicitement (`std::optional`), ce que le compilateur impose.
- Si d'autres modules se mettent à créer des cartes (tests de règles en 1.3, par exemple), on réévaluera les
  constructeurs dédiés.
