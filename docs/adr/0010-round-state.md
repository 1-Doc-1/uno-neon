# 0010 — État de la manche : `Round` valeur pure, `RandomSource&` passé en paramètre

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
L'étape 1.2 introduit `Round`, l'agrégat d'une manche (SPEC §7.1). Le moteur a besoin d'aléatoire pendant la
manche : remélanger la défausse quand la pioche est vide, remettre un +4 retourné en premier dans la pioche. La SPEC
§7.2 donne la signature `Round::apply(PlayerId actor, const PlayerAction& action)`, sans `RandomSource`. §7.3 dit
que le `RandomSource` est « injecté par constructeur ». Garder une référence dans `Round` le rendrait dépendant d'un
objet extérieur, et ses copies partageraient la même source.

Il fallait aussi décider comment garantir les invariants de `Round` sans règles de jeu (étape 1.3).

## Décision
- `Round` ne stocke **aucune** dépendance : le `RandomSource&` est passé en paramètre à chaque opération qui en a
  besoin, `Round::start(RoundSetup, RandomSource&)` dès l'étape 1.2, et en 1.3
  `Round::apply(PlayerId actor, const PlayerAction& action, RandomSource& random)`. C'est un écart à la signature de
  §7.2, corrigée dans la SPEC avec un renvoi vers cette ADR.
- `Round` est une **valeur** : copiable, comparable (`operator==`), sans état partagé.
- **Fabrique qui valide** : le constructeur est privé ; `Round::start` renvoie `std::expected<Round, DomainError>`.
  Un `Round` qui existe respecte ses invariants : 2 à 10 joueurs distincts, un joueur courant assis, conservation
  des cartes (mains + pioche + défausse = deck reçu, identifiants uniques), défausse jamais vide.
- **Composition** : `Round` assemble des classes plus petites qui gardent chacune leur invariant. `TurnOrder` gère
  les sièges, le joueur courant et le sens. `DrawPile` et `DiscardPile` sont les piles (la défausse est construite
  avec sa première carte, elle n'est donc jamais vide). `drawCards` est une fonction libre qui pioche et remélange.
- Le deck est reçu **déjà mélangé**, dans l'ordre de pioche : c'est l'appelant qui mélange (le futur `Match`,
  étape 1.5). Les tests peuvent ainsi construire un deck précis, par exemple un +4 retourné en premier.
- `PlayerId` enveloppe une `std::string`, comme le `PlayerId` opaque du protocole. Le cœur ne fait que comparer
  les identifiants ; c'est la couche des sessions qui les génère.
- Si toutes les cartes restant dans la pioche sont des +4, la première carte ne peut pas être retournée :
  `start` renvoie `DomainError::NoValidStartingCard` au lieu de boucler (impossible avec le deck officiel).

## Alternatives écartées
- **Référence (`std::reference_wrapper<RandomSource>`) injectée à la construction**, conforme à la lettre de §7.2
  et §7.3 : `Round` ne serait plus une valeur pure. Le `RandomSource` devrait vivre plus longtemps que chaque
  `Round`, et deux copies partageraient la même source : une copie faite pour explorer un coup (simulation, bots)
  ferait avancer l'aléatoire de l'original et casserait la relecture.
- **Une seule classe `Round` qui gère tout** : les règles des sièges et du remélange y seraient mélangées ; chacune
  est ici testée seule.
- **Mélanger le deck dans `Round::start`** : on ne pourrait plus tester un ordre de pioche précis.

## Conséquences
- Rejouer une partie ne demande que la graine et la liste des actions, et comparer deux états revient à comparer
  deux valeurs (`==`).
- Chaque appel au moteur qui peut piocher doit recevoir le `RandomSource` : la couche `app` le passe explicitement.
- En 1.2, `Round` n'expose que des accès en lecture. Les mutations (`apply`) arrivent en 1.3 et devront conserver
  les mêmes invariants ; la simulation de l'étape 1.8 les vérifiera après chaque action.
