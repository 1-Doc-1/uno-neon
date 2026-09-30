# Protocole client ↔ serveur

Source de vérité du protocole WebSocket (voir `docs/SPEC.md` §8).

- `schema/` : schémas JSON Schema 2020-12. Toute évolution du protocole commence ici.
- `examples/valid/` : au moins un message valide par type de message.
- `examples/invalid/` : messages que le serveur doit rejeter. Le nom du fichier commence par le code d'erreur
  attendu : `UNKNOWN_TYPE.game-cheat.json`.

Les types TypeScript du client sont générés depuis ces schémas (`npm run protocol:gen` dans `client/`) ; les DTO C++
sont écrits à la main et vérifiés par des tests de contrat sur les exemples.
