# 0031 — Cinématique d'introduction et sons synthétisés, déclenchés par l'AnimationDirector

- **Statut** : accepté
- **Date** : 2026-10-10
- **Complète** : ADR 0022 (animations pilotées par les événements), ADR 0025 (rythme du jeu)
- **Remplace** : SPEC §13.3 d'origine (sons sous licence libre, coupés par défaut)

## Contexte
Le jeu n'avait ni son ni ouverture. Les sons doivent suivre l'animation (et pas l'état), ne poser aucune question de
licence, ne jamais gêner (autoplay des navigateurs, réglage personnel) ; l'ouverture doit rester courte, passable et
réduite en mouvement réduit, sans aucune ressource externe.

## Décision
- **Des sons synthétisés** : `audio/sounds.ts` décrit chaque son comme une courte liste de notes (oscillateur, hauteur,
  glissando, durée, intensité) ; `AudioService` les joue par la Web Audio API. Aucun fichier audio, donc aucune
  licence. Treize sons : poser, piocher, +2, +4, +5, Passe, Inversion, choix de couleur, UNO, contre-UNO, « c'est ton
  tour », victoire, défaite. Le +5 est un arpège cristallin aigu, la note la plus haute de tous les sons.
- **Un son = un effet** : `soundOfEffect(spec, meId)` (fonction pure, `features/table/fx/effect-sound.ts`) donne le son
  de chaque `EffectSpec` ; `AnimationDirector` le joue **au moment où l'effet démarre**, donc en phase avec l'animation,
  et jamais pour ce que la file a abandonné (vue complète, retard, onglet caché). Une pioche sonne carte par carte, au
  rythme du serveur. Aucun composant n'appelle l'audio pour un effet ; seul `SoundControl` règle le volume.
- **Seul le joueur concerné** entend « c'est ton tour », la victoire ou la défaite (`meId` passé au directeur avec
  les événements).
- **Autoplay** : le contexte audio n'est créé qu'au premier `pointerdown`, `keydown` ou `touchstart` (écoutés en
  capture sur le document). Avant, `play()` ne fait rien ; le son coupé ne crée même pas d'oscillateur.
- **Réglages personnels** : `enabled` (activé par défaut) et `volume` (60 % par défaut) sont des signals, mémorisés en
  `localStorage` (tolérant : navigation privée, stockage refusé). Changer le volume fait entendre un petit son. Le
  bouton « Son activé / Son coupé » et le curseur sont sur l'accueil et sur la table (icône seule en portrait étroit).
- **Testable** : la fabrique du contexte est un `InjectionToken` (`AUDIO_CONTEXT_FACTORY`) ; les tests y mettent un faux
  `AudioContext` qui note les oscillateurs.
- **Cinématique** : `Intro` (CSS et SVG, rien d'externe), au premier chargement de la session (`sessionStorage`) :
  les tuiles du logo tombent une à une, sept dos de cartes s'envolent en éventail, fondu à 2,7 s ; un clic ou une
  touche la passe ; `prefers-reduced-motion` la réduit à un fondu de 1,2 s sans cartes. Les navigateurs pilotés par un
  robot (`navigator.webdriver`) ne la voient pas, sinon elle recouvrirait l'accueil à chaque test E2E ; le test de la
  cinématique se fait passer pour un vrai navigateur.

## Alternatives écartées
- **Fichiers audio libres de droits** : licences à tracer, poids, et le calage avec les animations se fait mal.
- **Un son par événement du serveur** : ce serait ignorer ce que le directeur choisit de montrer (file abandonnée, vue
  complète) et le rythme réel ; un effet est le bon grain.
- **Jouer le son depuis les composants** : le synchronisme avec l'animation (une pioche carte par carte) et la règle
  « une seule source » seraient perdus.
- **Une cinématique en vidéo ou en Lottie** : une ressource externe de plus, pour 2,7 s.

## Conséquences
- `AnimationDirector` dépend d'`AudioService` (un service racine sans effet de bord avant une interaction) : ses tests
  existants ne changent pas.
- Les sons ne dépendent pas de `prefers-reduced-motion` (le son n'est pas un mouvement) ; le joueur les coupe d'un clic.
