# 0032 — Musique d'ambiance, réglages audio séparés, clic d'interface et poussière des effets

- **Statut** : accepté
- **Date** : 2026-10-10
- **Complète** : ADR 0031 (sons synthétisés et cinématique), ADR 0022 (animations pilotées par les événements)

## Contexte
L'ADR 0031 donnait des sons d'effets et une ouverture muette tant qu'aucun geste n'avait eu lieu. Il manquait : un geste
d'ouverture explicite (le navigateur refuse tout son avant), une ambiance musicale réglable à part, un retour sonore des
boutons, des malus plus marquants, et un peu de matière visuelle sur les effets les plus forts.

## Décision
- **Écran « Cliquer pour jouer »** (`Intro`) : au premier chargement de la session, un seul bouton plein écran ; son clic
  est le geste exigé par la règle d'autoplay, débloque l'audio (`AudioService.unlock`) et lance la cinématique **avec**
  un son d'ouverture (`intro`, ~2,7 s) calé sur sa chronologie. Une fois par session, comme la cinématique ; ignoré des
  navigateurs pilotés par un robot.
- **Deux circuits audio** dans l'`AudioService` : effets et musique ont chacun leur gain, leur volume (60 % / 30 % par
  défaut) et leur clé `localStorage`. Le « muet général » (`enabled`) coupe les deux.
- **Musique générée** (`audio/music.ts`) : une boucle de 16 battements (La mineur : Am – F – C – G), un coussin tenu et un
  arpège léger, planifiés par petits lots de 0,4 s d'avance (`MusicEngine`) ; aucun fichier, aucune licence. Elle ne
  démarre qu'après le premier geste, s'arrête à volume zéro ou en muet.
- **Tension de fin de tour** : `TableView` demande `setTension(true)` pendant les 5 dernières secondes **de mon** tour
  (une fois le tour ouvert). La tension demandée est lissée à chaque tick : le tempo passe de 68 à 132 battements par minute
  et la musique gagne un pouls grave et un tic aigu ; elle retombe en douceur à la fin. Désactivable (réglage personnel).
- **Clic d'interface** : une directive unique `ClickSound` (sélecteur `button`) joue `click`. Elle s'applique à tous les
  boutons des composants qui l'importent ; un test lit les sources et échoue si un gabarit contient un `<button>` sans
  l'import.
- **Malus plus marquants** : +2, +4, +5, Passe, Inversion et contre-UNO sont des sons à plusieurs couches (grondement,
  tonnerre, scintillement doré).
- **Poussière** : `ParticleSystem` (logique pure, 40 particules au plus) et `ParticleCanvas` (un seul `<canvas>`, boucle
  `requestAnimationFrame` qui s'arrête quand plus rien ne bouge). `EffectsLayer` lance un nuage quand il place un effet
  (+2, +4, +5 en or, Passe, Inversion, contre-UNO, contestation), à la couleur du token de l'effet. Rien en
  `prefers-reduced-motion`. Un effet = un son = des particules, tous déclenchés au même moment par le directeur.
- **Réglages** : panneau « Réglages audio » (`<details>` natif) sous le bouton Son, sur l'accueil et sur la table.

## Alternatives écartées
- **Une librairie audio ou des fichiers** : poids, licences, et la tension se règle mal sur une piste enregistrée.
- **Une directive à écrire sur chaque bouton** : l'oubli est le risque ; l'import par composant est vérifié par un test.
- **Un canvas par effet** : une boucle d'animation par effet, et aucun plafond global de particules.

## Conséquences
- Un joueur qui coupe le son ne fait créer aucun oscillateur (ni effet ni musique).
- Les effets sonores continuent d'être déclenchés **uniquement** par l'`AnimationDirector` ; la tension est le seul appel
  hors effet (ce n'est pas un effet de jeu mais un état du tour), et passe lui aussi par l'`AudioService`.
