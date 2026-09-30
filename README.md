# UNO Néon

Jeu de UNO multijoueur en ligne, jouable dans le navigateur sans installation ni compte : on choisit un pseudo, on
crée un salon, on partage un code de 6 caractères et on joue de 2 à 10 joueurs.

- **Serveur autoritaire** en C++23 (WebSocket, uWebSockets) : il décide de tout, le navigateur n'envoie que des
  intentions.
- **Client** Angular 22 (standalone, zoneless, signals).
- **Protocole** JSON sur `/ws`, décrit par des schémas JSON Schema.

> « UNO » est une marque de Mattel. Ce projet étudiant utilise un design de cartes original.

## Arborescence

| Dossier | Contenu |
|---|---|
| `protocol/` | schémas du protocole (source de vérité) et exemples de messages |
| `server/` | serveur C++23 : `uno_core` (règles, sans E/S), `uno_app` (salons, sessions), `uno_net` (réseau), `uno_server` |
| `client/` | application Angular |
| `e2e/` | tests de bout en bout Playwright (phase 4) |
| `deploy/` | Dockerfile, Caddy, docker-compose (phase 6) |
| `docs/` | spécification, avancement, décisions d'architecture (`adr/`) |

## Démarrer

1. Installer l'environnement : [docs/SETUP.md](docs/SETUP.md).
2. Les commandes de build, de test et de lint sont listées dans la section « Commandes » de [CLAUDE.md](CLAUDE.md).

## Documentation

- [Spécification](docs/SPEC.md) — le quoi et le pourquoi de chaque choix.
- [Avancement](docs/PROGRESS.md) — phases, étapes, journal.
- [Décisions d'architecture](docs/adr/) — une ADR par choix structurant.

## Contribuer

Une branche par étape (`feat/…`, `fix/…`, `chore/…`), commits au format
[Conventional Commits](https://www.conventionalcommits.org/fr/), fusion uniquement par pull request relue.
