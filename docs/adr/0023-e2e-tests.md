# 0023 — Tests E2E : une pile par test, graines trouvées par le protocole, aucune attente fixe

- **Statut** : accepté
- **Date** : 2026-10-03

## Contexte
Les tests de l'étape 4.4 doivent jouer de vraies parties dans des navigateurs (un contexte par joueur) contre le vrai
serveur. Il leur faut des parties **déterministes** (SPEC §17), donc `UNO_TEST_SEED` (ADR 0016) ; or le serveur utilise
un seul générateur pour les mélanges, les jetons de session et les codes de salon (ADR 0014) : la partie dépend de
l'*ordre* des connexions, pas seulement de la graine. Un test doit aussi durer moins de 30 s, ne jamais dormir, et ne
pas attendre une minute le forfait d'un joueur.

## Décision
- **Une pile par test** (`e2e/support/stack.ts`) : un serveur lancé avec sa graine sur un port libre, et le client
  construit servi par un petit serveur Node qui relaie `/ws` vers lui. Même origine, donc la vérification d'`Origin` est
  exercée telle quelle (`UNO_ALLOWED_ORIGINS` = l'adresse de la pile). Les limites de débit par adresse ne se partagent
  pas entre tests.
- **L'ordre des connexions est fixé** par la fixture `lobby` : Alice se connecte et crée le salon, puis Bob se
  connecte et le rejoint par le lien. Dans une partie à deux, Bob (l'invité) joue donc le premier.
- **Les graines se trouvent par le protocole** (`npm run seeds`, `e2e/tools/find-seeds.ts`) : le chercheur joue en
  WebSocket la même politique que les tests (accepter une pénalité, choisir rouge, jouer la première carte jouable de
  la main ; sinon le serveur pioche) et décrit ce que chaque graine donne. `support/seeds.ts` garde les graines
  retenues et ce qu'elles garantissent. Les scénarios dépendants d'une situation rare (UNO, contre-UNO, UNO
  obligatoire, fin de manche) la **déroulent avec la politique** (`playUntil`) jusqu'à l'état voulu au lieu de rejouer
  une liste de coups : le test lit l'écran à chaque tour.
- **Aucune attente fixe** : on attend des états visibles (rôles ARIA, textes, compteurs affichés). Pour attendre un
  changement d'écran, `expect.poll` sur une photographie de l'écran. Animations réduites (`reducedMotion`) : les
  effets deviennent de courts fondus (ADR 0022).
- **Crochets de test dans `uno_bootstrap`** (`server/bootstrap/`) : le choix de la source d'aléa et des délais est sorti
  de `main.cpp` pour être testable. `UNO_TEST_SEED` et `UNO_TEST_RECONNECT_GRACE_MS` (grâce de reconnexion raccourcie
  pour regarder un forfait sans attendre 60 s) n'existent que dans un binaire compilé avec `UNO_ENABLE_TEST_HOOKS`
  (preset `e2e`, serveur seul). Un test Catch2 (`test_hooks_test.cpp`) vérifie qu'un binaire sans les crochets **ignore
  `UNO_TEST_SEED`** (deux sources construites avec la même « graine » diffèrent) ; le CI vérifie en plus qu'aucun
  `UNO_TEST_` n'est présent dans le binaire de production.
- **CI** : un cinquième job, `e2e` (Linux, GCC), compile le serveur de test, construit le client et joue les scénarios.
  Les traces Playwright ne sont publiées en artefact qu'en cas d'échec. La fusion exige les cinq jobs verts.

## Conséquences
- Si le moteur change sa façon de mélanger ou de distribuer, ou si l'ordre dans lequel le serveur consomme son aléa
  change, les graines ne donnent plus les mêmes parties : les tests le disent (cartes attendues absentes) et
  `npm run seeds` en retrouve de nouvelles.
- Les scénarios de §17 qui demandent d'autres écrans (parties à quatre avec cumul, clavier seul, axe, captures de
  référence) restent à écrire : cette étape couvre les parcours de jeu.

## Alternatives écartées
- **Un serveur partagé par tous les tests** : la graine est celle du processus ; deux tests en parallèle se
  disputeraient le flux d'aléa.
- **Rejouer des listes de coups enregistrées** : fragile au moindre changement du moteur, illisible. Une politique et
  un état visé disent ce que le test veut montrer.
- **Un crochet qui force la main des joueurs** : il faudrait du code de test dans le moteur ; les graines n'en ont pas
  besoin.
