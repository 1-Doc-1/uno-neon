# Politique de sécurité

## Signaler une faille

Merci de **ne pas ouvrir d'issue publique** pour une vulnérabilité. Utilise le signalement privé de GitHub :
onglet **Security** du dépôt → **Report a vulnerability** (« Privately report a security vulnerability »).

Décris ce que tu as observé, comment le reproduire et l'effet possible. C'est un projet personnel : les réponses se
font quand je peux, sans délai garanti.

## Périmètre

Le serveur de jeu (`server/`), le protocole (`protocol/`) et le client (`client/`). Les invariants de sécurité du projet
sont décrits dans [CLAUDE.md](CLAUDE.md) et la [spécification](docs/SPEC.md) (§15) : serveur autoritaire, aucune
fuite de cartes cachées, entrées réseau hostiles.
