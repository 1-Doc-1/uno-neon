# 0022 — Animations : pilotées par les événements, purement cosmétiques

- **Statut** : accepté
- **Date** : 2026-10-03

## Contexte
Le serveur envoie, à chaque action, une vue complète (`PlayerView`) et les événements projetés qui l'ont produite
(ADR 0015). Animer à partir de la différence entre deux vues serait fragile : une carte jouée et une carte piochée au
même tour, un Joker, un +4 contesté donnent des différences ambiguës, et rien ne dit *qui* a fait *quoi*.

## Décision
- **Les événements pilotent les effets, jamais une comparaison de vues.** `planEffects` (fonction pure) traduit un lot
  d'événements en effets dans l'ordre du serveur ; l'état d'un lot à l'autre (qui joue, une pénalité attendue) lui est
  passé et rendu. L'état *affiché* vient toujours de la dernière vue : un effet se superpose, il ne retient jamais l'état
  (sauf deux retenues purement visuelles et bornées : la carte en vol est cachée jusqu'à son arrivée, la couleur du
  liseré attend la fin de la roue d'un Joker).
- **`AnimationDirector`** (service fourni par la table) consomme la file : un effet après l'autre, les petits se
  chevauchent, **un seul effet plein écran à la fois**. Il n'attend jamais le réseau et ne bloque jamais la saisie : la
  couche d'effets ne reçoit aucun clic.
- **On vide la file plutôt que de rattraper un retard** : vue complète (première vue, saut de `stateVersion`,
  reconnexion : `EventBatch.resync` dans le store), onglet passé en arrière-plan, ou plus de 2,5 s d'effets en attente.
  L'écran montre alors simplement la dernière vue.
- **Positions mesurées dans le DOM** (éléments `data-anchor`) au démarrage de chaque effet ; les cartes volent par
  FLIP avec l'API Web Animations, en n'animant que `transform` et `opacity`. Aucune dépendance. La main retient où était
  chaque carte : une carte jouée a déjà quitté la main quand l'effet démarre.
- **Mouvement réduit** : chaque effet devient un fondu de 160 ms. La règle globale de `base.scss` coupe toutes les
  animations CSS ; les effets réduits la contournent par leur classe (plus spécifique), les vols sont gérés à part.
- La rotation « désordonnée » de la pile de défausse est dérivée de l'identifiant de la carte (déterministe).

## Conséquences
- Un événement du protocole sans effet associé est simplement ignoré par `planEffects` ; en ajouter un revient à
  ajouter un cas.
- La démo `/dev/table?scenario=animations` rejoue une séquence scriptée par le même chemin que la production (vue +
  lot d'événements + director) ; elle est exclue du build de production et vérifiée par `check-prod-bundle`.

## Alternatives écartées
- **Différence de vues** : voir Contexte.
- **Animer avec des classes CSS dans chaque composant** : les vols de cartes traversent la table, donc plusieurs
  composants ; une couche unique au-dessus de la table est plus simple et ne perturbe pas la mise en page.
