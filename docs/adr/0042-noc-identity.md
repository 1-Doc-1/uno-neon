# 0042 — Nouvelle identité « NOC » : un nom, un logo et des cartes qui ne rappellent aucun jeu du commerce

- **Statut** : accepté
- **Date** : 2026-11-15
- **Remplace** : la partie « logo à tuiles colorées » de l'ADR 0021 et la ligne « Marque » de la SPEC §1
- **Concerne** : SPEC §1 (marque), §11.4 (cartes, logo), §12 (libellés), README, « Comment jouer », MODE-EMPLOI

## Contexte
Le jeu va être mis en ligne sur un serveur public. « UNO » est une marque de Mattel : ni le nom, ni les visuels
(tuiles colorées du logo, ovale incliné au centre des cartes, plaque rouge inclinée au dos) ne doivent rappeler le
jeu officiel. Les **règles ne changent pas**.

## Décision
1. **Nom** : le jeu s'appelle **NOC**. `APP_NAME = 'NOC'` (`client/src/app/core/app-name.ts`) pilote le logo, le dos
   des cartes, le titre de l'onglet, la cinématique d'introduction, l'écran « Cliquer pour jouer » et l'écran de fin.
   `index.html`, le manifeste (`manifest.webmanifest`) et le favicon (`favicon.svg`, `favicon.ico`) sont refaits.
2. **Annonces** : « UNO ! » devient **« Dernière ! »** et le « Contre-UNO » devient **« Contre ! »** (boutons, tampons,
   journal, messages d'erreur, aide, réglage « Dernière ! obligatoire »). Mêmes sons, mêmes durées, mêmes règles.
3. **Logo** : wordmark en SVG inline, lettres majuscules creuses au trait de néon (`--back-neon`) avec halo.
4. **Faces des cartes** : un **cercle** plus sombre cerclé d'un filet clair remplace l'ovale incliné.
5. **Dos des cartes** : fond sombre, anneau et wordmark « NOC » en tube de néon (plus de plaque rouge inclinée).
6. **Textes** : README et « Comment jouer » ne nomment plus le jeu d'origine ; une seule phrase neutre est permise
   (« NOC est un jeu de défausse inspiré des jeux de cartes classiques »). L'ancien avertissement Mattel est retiré.

## Variantes comparées (captures 1440 px dans `docs/img/rebrand/`)
- **Logo** : *tube* (retenu : trois lettres creuses, une seule couleur, lisible d'un coup d'œil, cohérent avec le bord
  néon des cartes) contre *lune* (lettres pleines blanches, le « O » devenu un croissant jaune : charmant, mais le O
  se lit moins bien comme une lettre et ajoute une seconde couleur de marque). Image : `variant-logos-tube-and-moon.png`.
- **Forme centrale** : *cercle* (retenu : tous les chiffres, « +2 » et icônes y tiennent sans toucher le bord)
  contre *losange* (le « +2 » du bas et les icônes débordent des pointes, les chiffres à deux traits y sont serrés).
  Images : `variant-cards-circle.png`, `variant-cards-diamond.png`.

## Avant / après
`before-accueil-1440.png`, `before-table-1440.png`, `before-podium-1440.png` (ancienne identité) contre
`accueil-1440.png`, `table-1440.png`, `podium-1440.png` et `rebrand/after-cards.png` (cartes, faces et dos).

## Recherche de « uno » (insensible à la casse) : ce qui reste, et pourquoi
Plus aucun texte visible par un joueur ni aucune page du README, de « Comment jouer » ou du mode d'emploi. Reste
uniquement, **comme dette technique à renommer plus tard** (renommer coûte cher et ne se voit pas) :

| Famille | Exemples | Pourquoi on les garde pour l'instant |
|---|---|---|
| Bibliothèques, exécutable, espace de noms, dossier d'en-têtes C++ | `uno_core`, `uno_app`, `uno_net`, `uno_server`, `uno_tests`, `namespace uno`, `server/*/include/uno/` | Renommer touche chaque `#include` et chaque `CMakeLists.txt` |
| Options CMake et variables d'environnement | `UNO_ENABLE_TEST_HOOKS`, `UNO_TEST_SEED`, `UNO_TEST_PACE_MS`, `UNO_TEST_RECONNECT_GRACE_MS`, `UNO_PORT`, `UNO_ALLOWED_ORIGINS`, `UNO_LOG_LEVEL`, `UNO_BIND_ADDRESS`, `UNO_TRUSTED_PROXY`, `UNO_SIMULATION_GAMES`, `UNO_SERVER_BIN`, `UNO_E2E_SCREENSHOTS`, `UNO_FIND_*`, `UNO_WEB_*`, `UNO_CSP_SCRIPT` | Contrat avec les scripts, le CI et la vérification « aucun crochet de test en production » |
| Noms du protocole (fil) | `game.callUno`, `game.catchUno`, `declareUnoToWin`, `canCallUno`, `mustDeclareUno`, `hasCalledUno`, `unoWindows`, événements `unoCalled` / `unoCaught`, raisons `MUST_DECLARE_UNO`, `UNO_WINDOW_CLOSED`, `UNO_GRACE_PERIOD`, exemple `uno-window` | Identifiants jamais montrés ; les renommer change le protocole v1 (schémas, exemples, types générés, tests de contrat) |
| Code du client | `UnoActions` (`uno-actions.*`, `app-uno-actions`), `forgotUno`, jeton CSS `--uno`, `kind="uno"` des boutons, son `uno`, effet `uno`, `LabelVariant 'uno'`, `GameStore.callUno` / `catchUno` | Identifiants internes alignés sur les noms du protocole |
| Stockage navigateur | cookie `uno_session`, clés `localStorage` `uno.nickname`, `uno.avatar`, `uno.sound.*`, `uno.music.*`, `uno.introSeen` | Les renommer ferait perdre pseudo et réglages aux joueurs existants, et invaliderait les sessions en cours |
| Dépôt, scripts, dossiers | dossier `uno-neon`, `uno-neon-linux`, `%TEMP%\uno-neon-online`, `uno_server.exe`, domaines factices `*.uno-neon.invalid` des tests | Chemins de travail ; le dépôt GitHub se renomme à part (voir la liste de fin de lot) |
| Historique | ADR 0011, 0018, 0019 (noms de fichiers et texte), entrées de journal de PROGRESS, journaux de décision | L'historique ne se réécrit pas ; les ADR gardent le vocabulaire de leur époque |
| Dépendances | `package-lock.json` (sous-chaînes d'empreintes, sans rapport) | Aucun sens à corriger |

## Conséquences
- Les tests, E2E et captures sont à jour (`uno.spec.ts` devient `last-card.spec.ts`, captures de téléphone du mode
  d'emploi reproductibles par `UNO_E2E_SCREENSHOTS=1 npx playwright test screenshots`).
- Un chantier « renommage technique » (bibliothèques, variables, protocole v2, stockage avec migration) pourra
  traiter les lignes du tableau ci-dessus, dans cet ordre de coût croissant : dépôt et dossiers, client, C++, fil.
