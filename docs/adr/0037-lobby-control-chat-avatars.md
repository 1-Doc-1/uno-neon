# 0037 — Retour au salon, exclusion en partie, avatars et chat

- **Statut** : accepté
- **Date** : 2026-10-11
- **Complète** : ADR 0014, 0016 ; la SPEC §5 (revanche), §8 et §11.5

## Décision
- **Fin de partie sans relance automatique** : à la fin, l'écran montre le classement et, **pour l'hôte seul**, « Revanche »
  (`match.rematch` : mêmes joueurs connectés, mêmes réglages) et « Retour au salon » (`room.backToLobby`) ; les autres
  voient « En attente de l'hôte ». En cours de partie, le menu de la table donne à l'hôte « Retour au salon », après
  une confirmation (la partie est perdue, sans score). `room.backToLobby` annule les minuteurs de la partie, oublie le
  match, ne laisse prêts que l'hôte et les bots et diffuse un `room.update` en phase `lobby`.
- **Exclusion** : `room.kick` est accepté par l'hôte à tout moment ; en partie, l'exclu est retiré comme un forfait
  (`removeFromRoom`). Côté client : un clic sur le profil (ligne du salon ou siège de la table) ouvre le menu du joueur,
  « Exclure » demande « Exclure Loïc ? ». Le serveur ne croit que l'hôte (`NOT_HOST`).
- **Avatars** : 12 personnages en SVG (`ui/avatar.ts`), aux couleurs des tokens. On n'envoie que `avatarId` (0 à 11) dans
  `room.create`, `room.join`, `room.createBotGame` (défaut 0, validé par le schéma et le codec) ; il est diffusé dans
  `RoomMember` et dans les sièges de la vue. Un bot prend un avatar libre. Le choix est mémorisé avec le pseudo
  (`localStorage`). Aucune image n'est envoyée ni acceptée.
- **Chat** : `chat.send {text}` → `chat.message {playerId, text}` à tous les humains du salon. Le serveur valide
  (`validateChatMessage` : UTF-8 strict, 1 à 200 caractères après trim, ni contrôle, ni invisible, ni changement de
  sens de lecture, ni usage privé) et limite à 5 messages par 10 s et par joueur (`RATE_LIMITED`). Le client affiche le
  texte avec une interpolation Angular (jamais `innerHTML`), garde 100 lignes, compte les non-lus d'un panneau replié
  et permet de masquer un joueur (pour soi, côté client). Les bots n'écrivent pas (ils n'ont pas de session).

## Alternatives écartées
- **Revanche automatique après un délai** : l'hôte ne peut plus décider, et les joueurs qui veulent partir sont pris de court.
- **Images d'avatar téléversées** : modération, stockage, surface d'attaque. Un numéro suffit.
- **Chat avec mise en forme (Markdown, liens)** : aucune valeur ici, un risque d'injection.

## Conséquences
- Codes d'erreur ajoutés : `SERVER_FULL`, `CHAT_INVALID` ; messages `room.backToLobby`, `chat.send`, `chat.message`.
- Le chat n'a pas d'historique côté serveur : un joueur qui arrive voit les messages à partir de son arrivée.
