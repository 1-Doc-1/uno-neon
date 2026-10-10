# 0033 — Jouer avec des amis par un tunnel, depuis le PC de l'hôte

- **Statut** : accepté
- **Date** : 2026-10-10
- **Complète** : SPEC §9.4, §9.5 et §18 (le déploiement permanent de la phase 6 reste à faire)
- **Numérotation** : le brief du lot S disait « ADR 0031 », mais ce numéro était déjà pris (cinématique et sons).

## Contexte
Avant la phase 6 (serveur permanent, nom de domaine, Docker), l'hôte veut faire jouer des amis à distance depuis son PC
Windows, avec une seule commande, sans ouvrir de port sur sa box et sans exposer plus que nécessaire.

## Décision
`scripts/play-online.ps1` assemble trois programmes locaux, tous liés à `127.0.0.1` :

```
ami ──https──▶ Cloudflare ──tunnel sortant──▶ cloudflared ──▶ Caddy :8080 ──▶ uno_server :9001
                                                              │ client compilé, /ws relayé
```

- **Un quick tunnel Cloudflare** (`cloudflared tunnel --url`) : connexion sortante seulement, aucun port ouvert, HTTPS
  fourni, adresse `*.trycloudflare.com` éphémère et sans compte. Le script lit l'adresse dans les logs du tunnel.
- **Caddy** (`deploy/Caddyfile.tunnel`) sert le client compilé, relaie `/ws` vers le serveur, compresse (zstd, gzip),
  met en cache un an les fichiers à empreinte (`immutable`), n'autorise pas la mise en cache de `index.html`, et ajoute
  les en-têtes de la SPEC §18 : CSP (`default-src 'self'`, `connect-src 'self' wss://{host}`, `frame-ancestors 'none'`…),
  HSTS, `nosniff`, `Referrer-Policy`, `Permissions-Policy`, COOP/CORP. Les scripts de la CSP sont les hachages qu'Angular
  calcule (`security.autoCsp`) : le script les lit dans l'`index.html` compilé.
- **Le serveur** écoute sur `127.0.0.1` seulement (nouvelle variable `UNO_BIND_ADDRESS`) ; `UNO_ALLOWED_ORIGINS` est
  l'adresse du tunnel, **seule** origine acceptée ; `UNO_TRUSTED_PROXY=true` pour que les limites par adresse voient les
  visiteurs et non Caddy : Caddy remplace `X-Forwarded-For` par l'adresse que Cloudflare donne dans `Cf-Connecting-Ip`
  (`trusted_proxies` limité à la boucle locale). Limites de débit et taille des messages : inchangées.
- **Ordre de démarrage** : Caddy → tunnel (pour connaître l'adresse) → serveur (qui a besoin de l'adresse pour son origine).
- **Contrôles avant d'afficher l'adresse** (`Assert-Secure`, le script refuse de publier si l'un échoue) : les ports ne
  sont ouverts qu'en `127.0.0.1`/`::1` ; le binaire ne contient pas `UNO_TEST_` (aucun crochet de test) ; le build client
  ne contient aucune page `/dev` (`check-prod-bundle.mjs`) ; la page publique a la CSP et les en-têtes ; `/health`
  répond 404 par le tunnel ; une origine étrangère est refusée au handshake WebSocket, celle du tunnel est acceptée.
- **Aucun orphelin** : un objet Job Windows (`KILL_ON_JOB_CLOSE`) contient les trois processus ; Ctrl+C passe par le
  `finally` (arrêt des arbres de processus), et si la fenêtre est fermée ou le script tué, Windows arrête quand même
  les trois. Le PC reste éveillé (`SetThreadExecutionState`) tant que le script tourne.
- **Installation** : `winget install --id CaddyServer.Caddy --exact` et `winget install --id Cloudflare.cloudflared
  --exact` ; `scripts/setup-online.ps1` vérifie (et avec `-Install` installe) les deux, et le reste de la chaîne.
- **Logs** : le serveur n'écrit jamais un jeton de session ni une adresse de client (test automatisé :
  `The logs of a whole session hold no session token and no client address`) ; ceux de Caddy et du tunnel vont dans
  `%TEMP%\uno-neon-online` et sont effacés au prochain lancement.

## Alternatives écartées
- **Ouvrir un port sur la box (redirection)** : expose la maison, exige une IP publique, pas de HTTPS.
- **ngrok, localtunnel** : compte ou limites, ou service moins fiable ; cloudflared est gratuit et sans compte.
- **Un tunnel nommé Cloudflare** (adresse stable) : demande un compte et un domaine ; c'est le sujet de la phase 6.
- **Servir le client par le serveur de jeu** : le serveur C++ ne sert que `/ws` et `/health` ; Caddy est de toute façon
  prévu en phase 6, autant utiliser la même brique.

## Conséquences
- L'adresse change à chaque lancement : l'hôte renvoie un nouveau lien à chaque soirée.
- Cloudflare voit le trafic en clair (il termine le TLS) : acceptable pour un jeu entre amis, sans compte ni secret.
- Sans nom propre, un ami ne peut pas « retrouver » la partie après l'arrêt : le salon vit tant que le script tourne.
