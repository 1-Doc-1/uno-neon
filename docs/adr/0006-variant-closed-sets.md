# 0006 — `std::variant` pour les ensembles fermés, interfaces pour les dépendances injectées

- **Statut** : accepté (appliqué à partir de la phase 1)
- **Date** : 2026-09-30

## Contexte
Le moteur manipule des ensembles **fermés** et connus à la compilation : les actions d'un joueur (`PlayCard`,
`DrawCard`, `Pass`…), les phases d'un tour (`AwaitingPlay`, `AwaitingColorChoice`…), les événements du domaine.
Le patron State/Command du GoF les modélise avec une classe de base virtuelle et une sous-classe par cas.

## Décision
- Ensembles fermés → `std::variant<...>` de structs simples, traités avec `std::visit` (et un visiteur
  `overloaded{...}`). Exemple : `TurnPhase = std::variant<AwaitingPlay, AwaitingDrawnCardDecision,
  AwaitingPenaltyResponse, AwaitingColorChoice, RoundOver>`.
- Ensembles **ouverts** ou dépendances à remplacer dans les tests → interface abstraite (classe à méthodes
  virtuelles pures) injectée par constructeur : `RandomSource`, `Scheduler`, `RoomRepository`, les politiques des
  options maison.
- **Aléatoire** : l'interface `RandomSource` vit dans `uno_core` (le moteur en a besoin pour mélanger) ;
  `CryptoRandomSource` (libsodium) vit dans la couche adaptateurs `uno_net`, la seule qui dépend de libsodium ;
  l'instance est créée et injectée dans `main.cpp` (racine de composition) ; `uno_app` ne connaît que l'interface.
  `SeededRandomSource` (`std::mt19937_64`) n'existe que pour les tests et la simulation.

## Pourquoi `std::variant` plutôt que des classes virtuelles ici
- **Exhaustivité vérifiée par le compilateur** : ajouter une phase sans la traiter dans un `std::visit` ne compile
  pas. Avec l'héritage, un cas oublié tombe dans un `default` ou une méthode de base, silencieusement.
- **Sémantique de valeur** : copie, comparaison (`==`) et affichage triviaux, donc des tests qui comparent des
  valeurs ; pas de `std::unique_ptr` ni d'allocation dynamique.
- **États invalides irreprésentables** : chaque alternative porte seulement les données utiles à sa phase (la
  pénalité en attente n'existe que dans `AwaitingPenaltyResponse`).
- La contrepartie (ajouter un cas oblige à toucher chaque `visit`) est un avantage pour un ensemble fermé : c'est
  exactement la liste des endroits à revoir.

## Alternatives écartées
- **Hiérarchie virtuelle (State du GoF)** : ouverte à l'extension, alors que l'ensemble est fermé par les règles ;
  allocations, pointeurs, et oubli d'un cas non détecté.
- **`enum class` + `switch`** : exhaustivité vérifiable (avec `-Wswitch`), mais les données propres à chaque cas
  doivent vivre à côté, souvent dans des `std::optional` qui peuvent être incohérents.

## Conséquences
- Les développeurs doivent connaître `std::visit` et le motif `overloaded` : un exemple commenté accompagnera la
  première utilisation en phase 1.
- `uno_core` reste sans dépendance externe ; la frontière core / adaptateurs est lisible dans les `CMakeLists.txt`.
