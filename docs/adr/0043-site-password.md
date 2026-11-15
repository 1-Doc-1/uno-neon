# 0043 — Accès protégé par mot de passe : `basic_auth` de Caddy, activé par `SITE_PASSWORD_HASH`

- **Statut** : accepté
- **Date** : 2026-11-15
- **Complète** : ADR 0033 (jouer par un tunnel), SPEC §9
- **Concerne** : `deploy/Caddyfile.tunnel`, `scripts/set-password.ps1`, `scripts/play-online.ps1 -Password`, MODE-EMPLOI

## Contexte
Le jeu est public (ADR 0042) mais une soirée entre amis ne doit pas être ouverte à quiconque tombe sur le lien du
tunnel. Il faut un verrou simple, sans compte ni base de données, qui couvre **tout** le site, WebSocket compris, et
qui ne fasse jamais traverser un mot de passe en clair par le dépôt, un journal ou une ligne de commande.

## Décision
1. **Le verrou est dans Caddy, pas dans le serveur de jeu.** Le serveur C++ reste inchangé : il n'a ni notion de mot de
   passe de site ni code supplémentaire à auditer. Caddy est déjà la seule porte d'entrée (ADR 0033).
2. **`basic_auth` avec un hachage bcrypt.** Si la variable d'environnement `SITE_PASSWORD_HASH` est définie et non vide,
   Caddy exige le nom d'utilisateur `noc` et le mot de passe correspondant pour **toute** requête : la page, les
   fichiers, `/health` et la poignée de main `/ws`. Un navigateur retient les identifiants pour l'origine et les renvoie
   à la poignée de main WebSocket (vérifié par un test E2E : partie complète derrière le mot de passe).
3. **Sans la variable, rien ne change.** Le bloc est gardé par un matcher `expression "{$SITE_PASSWORD_HASH:}" != ""` :
   variable absente, le matcher est faux et l'authentification n'est jamais évaluée. La valeur par défaut du hachage
   (`bm9uZQ==`, « none » en base64) ne sert qu'à garder la configuration valide ; elle n'est jamais comparée. **Choix
   sûr par défaut du côté « protégé »** : définir la variable suffit, aucune seconde variable à ne pas oublier (un
   interrupteur séparé aurait pu laisser le site ouvert par erreur).
4. **Aucun mot de passe en clair, nulle part.**
   - `scripts/set-password.ps1` lit le mot de passe deux fois sans l'afficher, le passe à `caddy hash-password` par
     l'**entrée standard** (jamais en argument : une ligne de commande se voit dans la liste des processus) et n'écrit
     que le hachage, dans `%LOCALAPPDATA%\noc\site-password.hash`, **hors du dépôt**.
   - `play-online.ps1 -Password` lit ce hachage (ou `SITE_PASSWORD_HASH` s'il existe) et ne le donne qu'au processus
     Caddy. Sans `-Password`, une variable `SITE_PASSWORD_HASH` oubliée dans la session est **retirée** avec un
     avertissement : le script dit toujours vrai sur l'état de la soirée.
   - Caddy ne journalise ni l'environnement ni les identifiants (journal d'accès désactivé).
5. **Contrôles du script en mode protégé** : avant d'afficher le lien, `play-online.ps1` vérifie que la page publique
   répond 401 sans identifiants et que le WebSocket est refusé ; il n'a pas le mot de passe en clair, le parcours « avec
   mot de passe » est donc joué par les tests E2E.

## Tests
`e2e/tests/password.spec.ts`, derrière le **vrai** `deploy/Caddyfile.tunnel` (même CSP, mêmes en-têtes) :
sans mot de passe, la page (`/`, `/index.html`, `/comment-jouer`, `/health`) et `/ws` répondent 401 ; un mauvais mot de
passe est refusé partout ; le bon donne 200 et `101 Switching Protocols` ; une partie complète à deux navigateurs se
joue normalement. Le CI installe Caddy à une version et une empreinte SHA-512 épinglées.

## Limites assumées
- **Un seul mot de passe partagé** : pas de comptes, pas de révocation individuelle (on en change pour la soirée
  suivante). C'est voulu : le public visé est « mes amis ».
- **Pas de limitation des essais** au niveau de Caddy : seul le coût de bcrypt (facteur 14 : un essai coûte du temps de calcul à Caddy) ralentit une attaque, ce qui impose un mot de passe qui n'est pas un mot courant (le mode d'emploi le dit).
  Une vraie limitation de débit par adresse serait un module Caddy supplémentaire : à voir avec le déploiement
  permanent (phase 6).
- **Le tunnel du lot S transporte les identifiants en HTTPS** (Cloudflare termine TLS) ; un déploiement direct devra
  garder HTTPS obligatoire (Caddy automatique), sinon `basic_auth` enverrait le mot de passe en clair.
- Les identifiants protègent l'accès au site, pas la partie : le serveur reste la seule autorité sur les règles et ne
  fait pas confiance au navigateur, mot de passe ou pas (invariants de `CLAUDE.md`).

## Alternatives écartées
- Un mot de passe de salon dans le serveur de jeu : il faudrait un changement de protocole et ne protégerait ni la page
  ni `/health`.
- Une page de connexion dans l'application : du code de sécurité à écrire et à tester là où `basic_auth` existe déjà.
- Un interrupteur `SITE_AUTH=on` séparé du hachage : une variable oubliée laisserait le site ouvert sans le dire.
