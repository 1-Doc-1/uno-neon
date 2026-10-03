# 0021 — Direction artistique « Nuit » : l'esprit du jeu de cartes, le néon en réserve

- **Statut** : accepté (thème « Nuit » retenu au lot I ; le thème « Tapis » du lot H est supprimé)
- **Date** : 2026-10-03
- **Remplace** : la direction « Néon Crépuscule » de la SPEC §11 (version initiale).

## Contexte
Le premier habillage prenait le néon trop à cœur (fond de coucher de soleil, grille, verre, halos partout, Orbitron) :
il était chargé et ne faisait pas penser à un jeu de cartes. Le lot H a comparé deux thèmes, « Tapis » (rouge très
sombre) et « Nuit » (bleu nuit) ; « Nuit » est retenu.

## Décision
- **Néon réservé** : bords des cartes, carte jouable, dessus de la défausse, boutons primaire / destructif / UNO /
  Contre-UNO. Le reste est sobre, sans halo.
- **Un seul thème, « Nuit »** (anthracite / bleu nuit très sombre). Les tokens vivent dans `:root` : un autre thème se
  limiterait à les redéfinir sous un sélecteur, sans toucher aux composants. Pas de commutateur ni de paramètre `?theme`
  tant qu'il n'y a qu'un thème (pas de code mort).
- **Panneaux en verre sombre** : fond translucide, `backdrop-filter: blur`, liseré clair très fin en haut, ombre douce, un
  seul niveau d'élévation, repli opaque via `@supports`. Jamais de flou sur les cartes ni sur de grandes surfaces
  animées (coût GPU, surtout sur mobile).
- **Cartes** : fond plein de la couleur (saturée, pas fluo), chiffre blanc à contour sombre, bord néon de la couleur du
  jeu. Une carte non jouable reste **opaque** et s'atténue par la luminosité et la saturation, jamais par `opacity` (la
  carte du dessous apparaissait à travers). Design original, pas une copie des cartes officielles.
- **Typographie** : Fredoka (display ronde et grasse) + Inter, auto-hébergées, licence OFL ; JetBrains Mono pour le code.
- **Logo** : wordmark en SVG inline, une tuile par lettre de `APP_NAME` ; icônes en composant SVG maison. Aucune
  dépendance ajoutée hors polices.
- **Disposition de la table** : une fonction pure `seatLayout(nombreAdversaires)` donne les positions et rotations des
  sièges sur une ellipse (testée), plutôt qu'un cas CSS par nombre de joueurs ; en portrait étroit, un arc plat (jusqu'à
  4 adversaires) ou une bande défilante. Moi en bas, ma pastille en bas à gauche.
- **Architecture** : `TableView` est un composant de présentation (entrées / sorties seulement) ; `Table` est le
  conteneur qui l'unit au `GameStore`. La page `/dev/table?scenario=…` nourrit `TableView` de situations écrites à la
  main (fenêtre de contre-UNO, contestation, UNO obligatoire, tables de 2 à 7 joueurs), pour les captures et les revues.
- **Pages de développement hors production** : `app.routes.ts` importe `DEV_ROUTES` de `dev/dev.routes.ts`, que le build
  de production remplace par `dev.routes.prod.ts` (liste vide, `fileReplacements`). Ni les routes, ni les fixtures, ni
  les composants de ces pages ne sont dans le bundle. `scripts/check-prod-bundle.mjs` (étape du CI) le vérifie sur le
  JavaScript publié, et prouve à vide qu'il lit bien l'application ; un test unitaire fixe le contenu des deux listes.

## Alternatives écartées
- **Garder le verre et le néon en les atténuant** : le défaut était de structure (tout brille), pas de dosage.
- **Une bibliothèque d'icônes** : une douzaine de tracés suffisent ; pas de dépendance pour cela.
- **Cacher les routes `/dev` par `isDevMode()`** : le code des pages resterait dans le bundle publié.

## Conséquences
- Les tests de l'accueil, du salon (info-bulle, `aria-disabled`, vue hôte / invité), de `seatLayout` et de la table
  suivent ces comportements ; un test serveur vérifie qu'un non-hôte ne change ni réglages ni membres.
- Contrastes WCAG 2.2 AA mesurés avec axe (`scripts/axe-snippet.js`) sur l'accueil, le salon et la table, et
  `prefers-reduced-motion` vérifié par émulation dans le navigateur (aucune animation ne tourne).
- La galerie complète (`/dev/gallery`, étape 3.6) reprendra `TableView` et ses scénarios.
