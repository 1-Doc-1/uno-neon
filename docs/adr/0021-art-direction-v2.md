# 0021 — Nouvelle direction artistique : l'esprit du jeu de cartes, le néon en réserve

- **Statut** : accepté, en attente du choix du thème (PR du lot H non fusionnée)
- **Date** : 2026-10-03
- **Remplace** : la direction « Néon Crépuscule » de la SPEC §11 (version initiale).

## Contexte
Le premier habillage prenait le néon trop à cœur (fond de coucher de soleil, grille, verre, halos partout, Orbitron) :
il était chargé et ne faisait pas penser à un jeu de cartes.

## Décision
- **Néon réservé** : bords des cartes, carte jouable, dessus de la défausse, boutons primaire / destructif / UNO /
  Contre-UNO. Le reste est sobre : panneaux unis sans bordure ni halo, **une seule élévation** (`--elevation`).
- **Typographie** : Fredoka (display ronde et grasse) + Inter, auto-hébergées, licence OFL ; JetBrains Mono pour le code.
- **Logo** : wordmark en SVG inline, une tuile par lettre de `APP_NAME` ; icônes en composant SVG maison. Aucune
  dépendance ajoutée hors polices.
- **Deux thèmes** à structure de tokens identique, en `[data-theme]` : **A « Tapis »** (rouge très sombre, panneaux
  anthracite) et **B « Nuit »** (bleu nuit, accents des quatre couleurs). Le choix reste à faire ; les deux restent
  livrés tant qu'il n'est pas fait. Le thème par défaut est « Tapis ».
- **Mises en page** : accueil à un panneau ; salon en deux colonnes avec un grand bouton central dont l'état bloqué est
  expliqué par une info-bulle (`aria-disabled`, `aria-describedby`) ; table avec adversaires en arc et éventails de dos,
  ma main en éventail, pile d'annonces (UNO, un Contre-UNO par cible).
- **Architecture** : `TableView` est un composant de présentation (entrées / sorties seulement) ; `Table` est le
  conteneur qui l'unit au `GameStore`. La page `/dev/table?scenario=…&theme=…` nourrit `TableView` de situations écrites à
  la main (fenêtre de contre-UNO, contestation, UNO obligatoire, table pleine), ce qui sert aux captures et aux revues.

## Alternatives écartées
- **Garder le verre et le néon en les atténuant** : le défaut est de structure (tout brille), pas de dosage.
- **Une bibliothèque d'icônes** : une douzaine de tracés suffisent ; pas de dépendance pour cela.
- **Un seul thème** : deux jeux de tokens prouvent que rien n'est codé en dur, et laissent choisir.

## Conséquences
- Les tests de l'accueil, du salon (info-bulle, `aria-disabled`, vue hôte / invité) et de la table suivent ces
  comportements ; un test serveur vérifie qu'un non-hôte ne change ni réglages ni membres.
- La galerie complète (`/dev/gallery`, étape 3.6) reprendra `TableView` et ses scénarios.
