# Déploiement de NOC sur Coolify — pas à pas

Ce guide met NOC en ligne **en permanence** sur ton VPS (Coolify v4 et son proxy Traefik), à l'adresse
`https://noc.62-83-85-91.sslip.io`. Pour une soirée ponctuelle depuis ton PC, sans serveur, c'est le
[mode d'emploi](MODE-EMPLOI.md) (tunnel). Les décisions sont dans l'[ADR 0044](adr/0044-coolify-deployment.md).

> Les noms des écrans de Coolify sont décrits d'après sa documentation v4. Selon ta version, un intitulé peut différer un
> peu (le sens reste le même) : en cas de doute, la section « Dépannage » en fin de guide dit quoi regarder.
> **Je ne te demande jamais de mot de passe, de clé ni de secret** : tu les saisis toi-même, dans Coolify et dans GitHub.

## Ce qu'il faut savoir avant

**Comment ça marche.** À chaque fusion dans `main`, GitHub Actions construit deux images (le client avec Caddy, et le serveur
de jeu), les publie sur GHCR (le registre d'images de GitHub, gratuit pour un dépôt public) sous un tag `sha-<commit>`.
Coolify **ne construit rien** : il télécharge ces images. Ton VPS ne compile donc jamais de C++ (ce qui aurait pris des
Go de mémoire et gêné NIDO).

```
visiteur ──HTTPS──▶ Traefik (Coolify) ──▶ web (Caddy, port 8080) ──▶ server (jeu, réseau interne)
```

- **Aucun port n'est publié** : seul Traefik ouvre 80 et 443. Le serveur de jeu n'a pas de domaine et n'est joignable que par Caddy.
- **Mémoire** : environ 50 à 60 Mo au repos pour les deux conteneurs, limités à 128 Mo (serveur) et 96 Mo (Caddy) pour protéger NIDO.
- **Les parties sont en mémoire.** Un redéploiement (mise à jour, redémarrage du serveur) **efface toutes les parties en cours
  et les salons**. Les joueurs reviennent à l'accueil. **Préviens-les avant de redéployer** (« mise à jour dans 2 minutes »)
  et attends la fin des manches. Il n'y a pas de base de données, par choix.
- **Vie privée (RGPD)** : les journaux ne contiennent ni adresse IP, ni jeton de session, ni message de chat. Le jeu ne garde que
  des pseudonymes, en mémoire.

## 1. Rendre les images téléchargeables (GitHub)

Après la **première** publication (onglet *Actions* du dépôt → workflow *Publish images* : les deux jobs doivent être verts),
GitHub crée deux paquets **privés** par défaut. Coolify ne peut pas les télécharger tant qu'ils sont privés.

Pour **chacun des deux** paquets, `noc-web` et `noc-server` :

1. GitHub → ta photo de profil → **Your profile** → onglet **Packages** → clique le paquet.
2. En bas à droite : **Package settings**.
3. Tout en bas, **Danger Zone** → **Change visibility** → **Public** → tape le nom du paquet pour confirmer.

> Le code est public ; les images ne contiennent ni secret ni donnée (voir le dépôt : aucun secret n'y est autorisé).

Note le **tag** de la dernière publication : sur la page du paquet, le tag qui ressemble à `sha-1a2b3c4` (7 caractères). Tu en as
besoin à l'étape 4.

## 2. Autoriser le dépôt dans la GitHub App de Coolify

1. GitHub → ta photo de profil → **Settings**.
2. Menu de gauche → **Applications** → onglet **Installed GitHub Apps**.
3. Sur la ligne **lien-github-enzo** → **Configure**.
4. Rubrique **Repository access** → **Only select repositories** → **Select repositories** → choisis le dépôt du jeu.
5. **Save**.

## 3. Créer la ressource dans Coolify

1. Coolify → **Projects** → **+ Add** → nom **NOC** → **Continue**. Ouvre l'environnement **production**.
2. **+ New** → **Private Repository (with GitHub App)** → choisis ton serveur (*localhost*) puis la source **lien-github-enzo**.
3. Sélectionne le dépôt du jeu, la branche **main**.
4. **Build Pack** : **Docker Compose**. **Base Directory** : `/`. **Docker Compose Location** : `/deploy/docker-compose.yml`.
5. **Continue**.

Sur la page de configuration de la ressource :

6. **Vérifie la ligne « Resolved file »** : elle doit afficher **`/deploy/docker-compose.yml`**. Le *Smart scan* de Coolify peut en
   choisir un autre (un autre fichier Compose du dépôt) : corrige l'emplacement si besoin et **recharge le fichier Compose**
   (bouton *Reload Compose File*). Tu dois voir deux services : **`server`** et **`web`**.
7. **Domains for web** (le domaine se règle **uniquement sur `web`**, jamais sur `server`) : saisis
   `noc.62-83-85-91.sslip.io:8080`
   - le nom de domaine, **deux-points**, puis le **port interne de Caddy : 8080** ;
   - **sans** « https:// » et **sans** « // » ; le protocole est **https** (Coolify demande le certificat à Let's Encrypt, Traefik le sert) ;
   - si ta version de Coolify te fait choisir le protocole dans un sélecteur, prends **https**.
8. **Laisse le champ Domains de `server` vide.**
9. **Désactive le déploiement automatique.** Onglet **Advanced** (ou **General** selon la version) → décoche **Auto Deploy**.
   *Pourquoi* : sinon, à chaque push, Coolify relancerait l'**ancienne** image `latest` pendant que GitHub construit encore la
   nouvelle. C'est le workflow GitHub qui déclenche le redéploiement, **après** la publication (section 8). Ne configure pas non
   plus de webhook GitHub → Coolify.
10. **Save**.

## 4. Variables d'environnement

Onglet **Environment Variables** de la ressource. Coolify détecte les variables du fichier Compose ; ajoute (ou complète) :

| Variable | Valeur | Obligatoire |
|---|---|---|
| `NOC_IMAGE_TAG` | le tag publié, par exemple `sha-1a2b3c4` (étape 1) | **oui** |
| `NOC_PUBLIC_ORIGIN` | `https://noc.62-83-85-91.sslip.io` (avec `https://`, **sans** `/` final) | **oui** |
| `SITE_PASSWORD_HASH` | vide au début ; voir la section 5 | non |
| `NOC_TRUSTED_PROXIES` | vide au début ; voir « Resserrer la confiance » plus bas | non |
| `NOC_LOG_LEVEL` | `info` (par défaut) | non |

- `NOC_PUBLIC_ORIGIN` est la **seule** origine acceptée par le serveur pour les WebSocket (jamais « tout accepter »).
- Si une variable obligatoire manque, le déploiement s'arrête avec un message qui dit laquelle.
- Le fichier [`deploy/.env.example`](../deploy/.env.example) explique chaque variable.

Puis **Deploy**. Les journaux de déploiement doivent finir par deux conteneurs **Healthy** (`server`, puis `web`).

### Vérifier une fois que le serveur n'est PAS relié à Traefik

(Coolify ne rattache pas un service sans domaine au réseau de Traefik, mais ce comportement n'a pas pu être vérifié avant la mise en ligne : fais-le une fois, en SSH sur le VPS.)

```bash
# Les réseaux de chaque conteneur : server n'a QUE le réseau « noc-internal », web en a deux
docker ps --format '{{.Names}}' | grep -E '^(server|web)-'
docker inspect -f '{{range $n,$_ := .NetworkSettings.Networks}}{{$n}} {{end}}' <nom du conteneur server>
docker inspect -f '{{range $n,$_ := .NetworkSettings.Networks}}{{$n}} {{end}}' <nom du conteneur web>

# Traefik ne doit PAS apparaître sur le réseau « …noc-internal » (il apparaît sur celui de web, c'est normal)
docker network inspect <réseau noc-internal> --format '{{range .Containers}}{{.Name}} {{end}}'

# Aucun port publié, et les limites de mémoire
docker ps --filter name=server- --filter name=web- --format '{{.Names}} {{.Ports}}'
docker stats --no-stream $(docker ps -q --filter name=server- --filter name=web-)
```

Attendu : `server` n'est que sur `…noc-internal`, la liste des machines de ce réseau ne contient que `server` et `web`, aucun
`->` dans les ports, et la colonne *MEM USAGE / LIMIT* montre environ 10 à 40 Mo sur 128 Mo / 96 Mo.

**Si le déploiement échoue avec « Pool overlaps »** : un autre réseau Docker du VPS utilise déjà `10.231.77.0/28` (voulu : ça
échoue bruyamment plutôt que de se mélanger). Liste les sous-réseaux pris
(`docker network ls -q | xargs docker network inspect --format '{{range .IPAM.Config}}{{.Subnet}} {{end}}'`), choisis un /28
libre, puis fixe **ensemble** trois variables : `NOC_INTERNAL_SUBNET` (ex. `10.231.90.0/28`), `NOC_WEB_ADDRESS` (`10.231.90.2`) et
`NOC_SERVER_ADDRESS` (`10.231.90.3`). Les deux adresses doivent être dans le sous-réseau.

## 5. Activer le mot de passe (facultatif)

Sans mot de passe, tout le monde qui connaît l'adresse peut jouer. Avec, le navigateur demande un nom d'utilisateur
(**`noc`**) et le mot de passe avant tout (page et WebSocket). Fonctionnement : [ADR 0043](adr/0043-site-password.md).

1. Sur ton PC, dans PowerShell, à la racine du projet : `./scripts/set-password.ps1 -Coolify`
2. Tape le mot de passe deux fois (8 caractères au moins ; une phrase de passe vaut mieux qu'un mot).
3. Le script affiche **le hachage** (pas le mot de passe) sous deux formes. Dans Coolify → **Environment Variables** → crée
   (ou modifie) `SITE_PASSWORD_HASH` :
   - **Forme 1 (recommandée)** : colle le hachage **tel quel** (`$2a$14$…`) et **coche « Literal »** sur la variable ;
   - **Forme 2** : sans « Literal », colle la version où **chaque `$` est doublé** (`$$2a$$14$$…`).
4. **Redeploy**.

**Pourquoi ces précautions.** Un hachage bcrypt contient des `$`. Docker Compose les prend pour des variables et abîme la valeur
si on ne les protège pas : le mot de passe ne marcherait plus. Les deux formes ci-dessus sont **testées** à chaque modification
(job CI `deploy-smoke`). Et si malgré tout le hachage arrive abîmé, le conteneur `web` **refuse de démarrer** en l'expliquant dans
ses journaux (« SITE_PASSWORD_HASH n'est pas un hachage bcrypt valide ») : **le site ne reste jamais ouvert par erreur**.

**Changer** le mot de passe : refais les étapes 1 à 4. **Désactiver** : vide `SITE_PASSWORD_HASH`, puis **Redeploy**.
**Partager** : envoie à tes amis le lien et le mot de passe **par deux canaux différents** ; nom d'utilisateur : `noc`.
`/sante` reste ouverte (elle ne révèle rien) : c'est elle que la vérification de santé interroge.

## 6. Vérifier que tout marche

1. **Santé** : ouvre `https://noc.62-83-85-91.sslip.io/sante` → la page affiche **`ok`** (en HTTPS, cadenas valide).
   Depuis un terminal : `curl -i https://noc.62-83-85-91.sslip.io/sante` → `HTTP/2 200`.
2. **`/health` est fermé** : `curl -i https://noc.62-83-85-91.sslip.io/health` → `404` (c'est voulu).
3. **Une partie à plusieurs** : ouvre le jeu dans **deux navigateurs** (ou une fenêtre privée, ou deux téléphones sur la 4G) :
   l'un crée un salon, l'autre le rejoint par le code, lancez une manche. (Deux onglets du même navigateur sont le **même** joueur.)
4. **Reconnexion** : en pleine manche, ferme l'onglet d'un joueur puis rouvre le lien dans le **même navigateur** : il retrouve
   sa place (dans la minute qui suit).
5. **Mot de passe (si activé)** : une fenêtre privée demande `noc` et le mot de passe ; un mauvais mot de passe est refusé.
6. **Les adresses sont bien distinctes** : depuis deux appareils différents, créez chacun des salons sans erreur
   « trop de demandes » : les limites s'appliquent par joueur, pas à tous ensemble.

## 7. Resserrer la confiance dans Traefik (recommandé)

Par défaut, Caddy croit l'adresse du visiteur transmise par Traefik **si** elle vient de la plage `172.16.0.0/12` (les réseaux
Docker). Sur le réseau de la ressource, Caddy n'a que Traefik pour voisin, donc c'est sûr ; tu peux quand même la limiter au
sous-réseau exact :

```bash
docker inspect -f '{{range $n,$_ := .NetworkSettings.Networks}}{{$n}} {{end}}' <nom du conteneur web>   # repère le réseau SANS « noc-internal »
docker network inspect <ce réseau> --format '{{range .IPAM.Config}}{{.Subnet}}{{end}}'                    # ex. 10.0.3.0/24
```

Mets ce sous-réseau dans `NOC_TRUSTED_PROXIES` (plusieurs plages : séparées par des virgules ; au moins `/8`, jamais « tout »),
puis **Redeploy**. Le réseau d'une ressource Coolify ne change pas d'un déploiement à l'autre.

## 8. Mettre à jour

Le chemin normal : **tu fusionnes une pull request dans `main`**. Le workflow *Publish images* construit et publie les images
(6 à 12 minutes), puis **redéploie Coolify tout seul** s'il connaît les deux secrets de l'étape « Automatique » ci-dessous.
Les changements de documentation seule ne publient rien.

### Automatique : donner au workflow le moyen de redéployer

**Dans Coolify**

1. **Settings** → **Advanced** → active **API Access** (si ce n'est pas déjà fait).
2. **Keys & Tokens** → **API tokens** → **Create** : un nom (`github-noc`) et des permissions qui permettent de **modifier une
   variable** et de **déployer** (en général `write` et `deploy`). **Copie le jeton affiché : il ne le sera qu'une fois.**
3. Ouvre la ressource NOC → **Webhooks** → copie l'adresse du **Deploy Webhook (auth required)** : elle ressemble à
   `https://<ton-coolify>/api/v1/deploy?uuid=<identifiant>&force=false`.

**Dans GitHub** (dépôt du jeu) : **Settings** → **Secrets and variables** → **Actions** → **New repository secret** :

| Nom du secret | Valeur |
|---|---|
| `COOLIFY_WEBHOOK_URL` | l'adresse du Deploy Webhook (étape 3) |
| `COOLIFY_TOKEN` | le jeton d'API (étape 2) |

À chaque publication, le workflow met d'abord `NOC_IMAGE_TAG` au tag `sha-<commit>` qu'il vient de publier, **puis** appelle le
webhook : le déploiement utilise donc exactement l'image publiée, jamais `latest`. Sans ces deux secrets, le workflow **publie
les images et saute le redéploiement, sans échouer**.

### Manuel (sans les secrets)

1. Attends que le workflow *Publish images* soit vert, puis note le nouveau tag (`sha-…`, étape 1).
2. Coolify → **Environment Variables** → `NOC_IMAGE_TAG` = ce tag → **Save**.
3. **Redeploy** (après avoir prévenu les joueurs : les parties en cours sont perdues).

### Revenir en arrière

Remets dans `NOC_IMAGE_TAG` le tag d'un déploiement qui marchait (les anciens tags restent sur la page du paquet GitHub), puis
**Redeploy**. C'est tout : c'est la raison d'être des tags `sha-…`.

## Dépannage

**502 Bad Gateway ou 504 Gateway Timeout**
- Le domaine pointe sur le **mauvais port** : il doit être `noc.62-83-85-91.sslip.io:8080` (port de Caddy), pas 9001 ni 80.
- `web` n'est pas *healthy* : ouvre les **Logs** du conteneur `web`. Un `web` qui redémarre en boucle explique presque toujours
  pourquoi (message « NOC (web) : … ») : hachage de mot de passe abîmé (section 5), `NOC_TRUSTED_PROXIES` invalide.
- `server` n'est pas *healthy* : `web` attend qu'il le soit. Logs du conteneur `server` (une origine invalide arrête le serveur
  au démarrage avec un message clair).
- Le domaine est mis sur `server` au lieu de `web` : retire-le, mets-le sur `web` seulement.

**Le WebSocket ne se connecte pas** (le jeu affiche « Connexion perdue » en boucle)
- Ouvre les outils du navigateur (F12) → onglet **Réseau** → `ws` : un code **403** = origine refusée (voir ci-dessous) ;
  **401** = mot de passe (un navigateur qui n'a pas gardé les identifiants : recharge la page) ; **502/504** = voir plus haut.
- Un proxy d'entreprise ou un antivirus qui coupe les WebSocket : essaie en 4G.

**« Origin refusée » (403 sur `/ws`)**
- `NOC_PUBLIC_ORIGIN` doit être **exactement** l'adresse affichée dans le navigateur : `https://noc.62-83-85-91.sslip.io`, avec
  `https://`, sans `/` final, sans port, sans `www`. Corrige, **Redeploy**.
- Tu ouvres le jeu par une **autre adresse** (l'adresse IP, un autre nom) : seule celle de `NOC_PUBLIC_ORIGIN` est acceptée.

**Mot de passe refusé alors qu'il est bon**
- Le hachage a été abîmé par les `$` : le conteneur `web` aurait dû refuser de démarrer (logs). Refais la section 5 en prenant la
  **forme 1 avec « Literal » coché**.
- Tu as saisi le mot de passe d'une **ancienne** soirée ou d'un **autre** hachage : relance `./scripts/set-password.ps1 -Coolify`,
  remets la valeur, **Redeploy**.
- Le nom d'utilisateur est **`noc`** (en minuscules).

**Conteneur « unhealthy »**
- `server` : sa vérification interroge `/sante` sur son propre port 9001. Regarde ses logs.
- `web` : sa vérification interroge `/sante` à travers **toute la chaîne** (Caddy puis le serveur) : `web` est « unhealthy » si
  `server` ne répond pas. Corrige d'abord `server`.
- Lecture des vérifications : Logs de la ressource ou `docker inspect --format '{{json .State.Health}}' <conteneur>`.

**Le déploiement dit « manifest unknown », « denied » ou « pull access denied »**
- Les paquets GHCR sont encore **privés** (étape 1) : passe-les en **Public**.
- `NOC_IMAGE_TAG` ne correspond à aucun tag publié : copie-le exactement depuis la page du paquet (`sha-` + 7 caractères).

**« NOC_IMAGE_TAG manque » ou « NOC_PUBLIC_ORIGIN manque »** : variable obligatoire non saisie (étape 4).

**« Pool overlaps with other one on this address space »** : section 4, sous-réseau déjà pris.

**Le redéploiement automatique ne se lance pas** : onglet *Actions* de GitHub → job *redeploy (Coolify)* : un message dit s'il manque un
secret, si l'adresse du webhook est mal formée ou si le jeton n'a pas le droit de modifier une variable. Les images, elles, sont
publiées dans tous les cas : tu peux alors faire la mise à jour **à la main**.

**Une ancienne version s'affiche encore** : vide le cache du navigateur (Ctrl+F5). Si `NOC_IMAGE_TAG` n'a pas changé, Coolify
redéploie la même image.

## Récapitulatif de ce que tu fais, une fois

1. Rendre les deux paquets GHCR publics.
2. Autoriser le dépôt dans la GitHub App.
3. Créer la ressource Compose, régler le domaine (sur `web`, port 8080), **désactiver Auto Deploy**.
4. Saisir `NOC_IMAGE_TAG` et `NOC_PUBLIC_ORIGIN` ; **Deploy**.
5. (Facultatif) mot de passe ; (facultatif) secrets `COOLIFY_WEBHOOK_URL` et `COOLIFY_TOKEN` pour le redéploiement automatique.
