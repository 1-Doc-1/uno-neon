# 0044 — Déploiement sur Coolify : images GHCR, Compose à deux conteneurs, chaîne de confiance exacte

- **Statut** : accepté
- **Date** : 2026-11-15
- **S'écarte de** : SPEC §18 (« Déploiement (phase 6) » : Caddy en HTTPS automatique, `Dockerfile.server` sur Debian slim construit sur le serveur, `DEPLOY.md`). Ici le TLS est celui de Traefik (Coolify), les images sont construites par GitHub Actions et le guide s'appelle `docs/DEPLOIEMENT.md`. La SPEC n'est pas modifiée (règle 8 de `CLAUDE.md`).
- **Complète** : ADR 0033 (tunnel), 0036 (session et plafonds par adresse), 0043 (mot de passe)
- **Concerne** : `deploy/`, `.github/workflows/publish-images.yml`, `server/net` (`TrustedProxies`)

## Contexte
NOC est mis en ligne sur un VPS qui héberge déjà d'autres applications, derrière le Traefik de Coolify (ports 80/443, TLS
automatique). Contraintes : aucun port publié par l'application, moins de 200 Mo de RAM, secrets seulement en variables
d'environnement, et surtout **rien qui puisse gêner les autres applications** (pas de compilation C++ sur le VPS).

## Décisions

### 1. Deux conteneurs, images construites par GitHub Actions et publiées sur GHCR
- `noc-web` : Caddy et le client compilé en production. Traefik le joint sur le port 8080 ; c'est le seul service avec un domaine.
- `noc-server` : le serveur de jeu, Release, **sans** `UNO_ENABLE_TEST_HOOKS`, libstdc++ et libgcc liées statiquement, image d'exécution `debian:trixie-slim`, utilisateur 10001.
- Construire sur le VPS aurait coûté 10 à 20 minutes de CPU partagé et 2 à 3 Go de RAM de pic (vcpkg, uWebSockets, spdlog),
  sur un serveur de 8 Go qui porte déjà Coolify et d'autres applications : un risque d'arrêt par manque de mémoire pour tout le monde.
  GitHub Actions est gratuit pour un dépôt public ; le VPS ne télécharge que quelques dizaines de Mo.
- Un seul conteneur aurait demandé un superviseur de processus : un changement d'architecture, non retenu.

### 2. Publication et redéploiement : une seule main, dans le bon ordre
- `publish-images.yml` publie `sha-<7 caractères du commit>` et `latest` à la fusion dans `main` (si le jeu a changé), puis
  **appelle Coolify** : met `NOC_IMAGE_TAG` au tag qui vient d'être publié (API d'environnement), puis le webhook de déploiement.
  Le redéploiement automatique de Coolify (« Auto Deploy ») est **désactivé** : sinon il relancerait l'ancienne image
  pendant que la nouvelle se construit.
- Le déploiement utilise le tag `sha-…`, jamais `latest` : revenir en arrière, c'est remettre un ancien tag dans `NOC_IMAGE_TAG` et redéployer.
- Secrets GitHub `COOLIFY_WEBHOOK_URL` et `COOLIFY_TOKEN` : absents, les images sont publiées et le redéploiement est sauté sans échec.
- Deux actions Docker seulement (`docker/login-action`, `docker/build-push-action`), épinglées par SHA. Pas d'`setup-buildx`
  ni de cache de couches : la construction est plus longue (6 à 12 minutes) mais n'ajoute aucune dépendance.

### 3. La chaîne de confiance : exactement les relais qui existent
`visiteur → Traefik → Caddy → serveur`. Chaque maillon ne croit que celui d'avant, et seulement pour `X-Forwarded-For` :
- **Traefik** pose l'adresse réelle du visiteur. **Caddy** ne le croit que si la connexion vient d'une plage de `NOC_TRUSTED_PROXIES`
  (plage Docker `172.16.0.0/12` par défaut, resserrable ; refusée si plus large qu'un /8) et lit l'adresse la plus à droite qui
  n'est pas un relais (`trusted_proxies_strict`). Il **réécrit** l'en-tête avec cette seule adresse.
- Le **serveur** ne croit l'en-tête que d'une connexion venue d'une adresse de `UNO_TRUSTED_PROXY_ADDRESSES` : ici l'adresse **exacte** de
  Caddy dans le réseau privé. Nouveau : jusqu'ici le serveur ne croyait que le loopback (Caddy à côté), ce qui, avec Caddy dans un autre
  conteneur, aurait compté tous les joueurs sous une seule adresse. `UNO_TRUSTED_PROXY` seul garde son sens (loopback, mode tunnel, ADR 0033).
- « Faire confiance à tout » n'est pas une configuration : `0.0.0.0/0` et tout ce qui est plus large qu'un /8 sont refusés (par le serveur
  et par l'entrypoint). Détail découvert en route : uWebSockets écrit les adresses IPv6 en forme complète
  (`0000:…:0001`), jamais `::1` ; la comparaison se fait donc sur les octets (IPv4-mappées comprises), pas sur le texte.
- Tests : unitaires (`TrustedProxies`, `clientAddress`), intégration (`websocket_server_test`), E2E avec le vrai
  `Caddyfile.production` (`proxy-chain.spec.ts` : 25 visiteurs distincts malgré une adresse forgée à gauche, limite de 20 pour un seul),
  et le même contrôle sur les vrais conteneurs (`deploy/smoke-test.sh`, job CI `deploy-smoke`).

### 4. Réseaux : le serveur de jeu n'est joignable que par Caddy
- Le serveur est sur `noc-internal` (`internal: true`, sous-réseau privé **/28** `10.231.77.0/28`) **seul avec Caddy**, sans domaine : pas de sortie, pas de port.
  Caddy y a l'adresse fixe `10.231.77.2` et est aussi sur le réseau par défaut (celui de Traefik).
- Un chevauchement avec un autre réseau Docker du VPS fait **échouer le déploiement** (Docker refuse de créer le réseau) : voulu. `NOC_INTERNAL_SUBNET`, `NOC_WEB_ADDRESS` et `NOC_SERVER_ADDRESS` se changent ensemble ; les deux conteneurs ont une adresse fixe (sans cela, Docker donne au serveur, démarré en premier, l'adresse prévue pour Caddy : défaut trouvé par le test de fumée).
- Le test de fumée vérifie que le serveur est sur ce seul réseau, qu'il est `internal`, qu'aucun port n'est publié et que chaque conteneur a une limite de mémoire.
- **À vérifier une fois dans Coolify** (je n'ai pas d'accès au serveur) : que `docker network inspect` ne montre pas Traefik sur `noc-internal` (commande dans le guide).

### 5. Mot de passe : fail-closed, et les « $ » d'un bcrypt
- Le conteneur web **vérifie** le format de `SITE_PASSWORD_HASH` au démarrage et refuse de démarrer s'il est abîmé : un site qu'on croyait protégé ne reste jamais ouvert.
- Docker Compose interprète les `$` d'un hachage (`$2a$14$…`). Deux formes sûres, **testées** par `smoke-test.sh` (hachage entre apostrophes,
  équivalent de « Literal » dans Coolify ; `$` doublés) ; la forme brute est vérifiée comme refusée. `set-password.ps1 -Coolify` affiche les deux.
- `/sante` reste ouverte (vérification de santé, ne révèle rien) ; tout le reste exige le mot de passe.

### 5 bis. Santé, durcissement, ressources
- `GET /sante` répond `ok` (rien d'autre) ; `/health`, qui donne les compteurs, reste fermé au public (404 par Caddy).
- `HEALTHCHECK` sans outil absent de l'image : `bash` et `/dev/tcp` pour le serveur, `wget` (busybox) pour Caddy.
- `read_only`, `cap_drop: ALL`, `no-new-privileges`, `tmpfs /tmp`, `mem_limit` (128 Mo serveur, 96 Mo Caddy avec `GOMEMLIMIT=64MiB`), `pids_limit`, journaux Docker bornés.
- La CSP (empreintes des scripts) est calculée à la **construction** de l'image web ; sans empreinte, la construction échoue.
- WebSocket : `idleTimeout` de 60 s avec ping/pong automatique de uWebSockets (toutes les 30 s environ) ; Traefik et Caddy ne coupent pas un WebSocket actif.
- RGPD : les journaux ne contiennent ni adresse IP, ni jeton, ni pseudo, ni message de chat (`playerId` aléatoire et code de salon seulement).
- L'état des parties est **en mémoire** : un redéploiement les efface (à dire aux joueurs avant, voir le guide). Pas de base de données.

## Limites assumées
- Non vérifié en vrai avant la mise en ligne : le comportement exact de Coolify (rattachement des réseaux, écriture du `.env`,
  droits du jeton d'API pour modifier une variable). Le guide donne les commandes de contrôle et le dépannage ; le CI prouve la partie Docker.
- `NOC_TRUSTED_PROXIES` par défaut couvre toute la plage `172.16.0.0/12` : sur le réseau par défaut de la ressource, Caddy n'a que Traefik pour voisin,
  mais le guide montre comment la resserrer au sous-réseau exact.
- Les images de base (`ubuntu`, `debian`, `node`, `caddy`) sont désignées par tag, pas par empreinte : à épingler quand un outil de mise à jour les suivra.

## Alternatives écartées
- Construire sur le VPS (risque pour les autres applications) ; un conteneur unique (superviseur) ; `trusted_proxies` « tout » côté serveur (forgeable) ;
  faire lire `X-Real-IP`/`CF-Connecting-IP` (propres à d'autres proxys) ; un réseau `internal` partagé avec Traefik (le serveur de jeu n'a rien à y faire).
