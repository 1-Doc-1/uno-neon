# 0039 — Jouer sur téléphone : mise en page, effets bornés, audio et clavier

- **Statut** : accepté
- **Date** : 2026-10-11
- **Complète** : ADR 0021 (direction artistique), 0032 (audio), 0038 (table)

## Décision
- **Mise en page** : la page ne défile jamais pendant la partie (`100dvh`, `overflow: clip` sur la table,
  `overscroll-behavior: none`, `touch-action: manipulation`, `viewport-fit=cover`). Le zoom au pincement reste permis :
  on corrige les débordements, on n'interdit pas de zoomer. Jusqu'à 3 adversaires en arc en portrait étroit, une bande
  défilante au-delà ; en paysage bas (≤ 480 px) les piles rapetissent et les commandes se réduisent à leurs icônes.
- **Effets bornés** : la directive `KeepOnScreen` ramène dans l'écran le centre de tout effet posé sur un point (grand
  texte, étiquette, tampon), d'après sa taille de mise en page mesurée (`keepInside`, fonction pure).
- **Audio** : le contexte est relancé à chaque geste s'il est suspendu ; `touchend` et `click` débloquent l'audio (seuls
  gestes acceptés par iPhone) ; une piste silencieuse d'**une seconde réelle** jouée en boucle fait passer iOS en
  « lecture » pour que l'interrupteur de sonnerie ne coupe pas la page (documenté dans `MODE-EMPLOI.md` : il peut quand
  même la couper).
- **Tests** : parties jouées par émulation iPhone 13 et Pixel 7, portrait et paysage (75 s de jeu chacun ; la partie entière est jouée par
  les tests au clavier et contre les bots), qui mesurent à chaque étape l'absence de défilement, la position des sièges et celle de chaque effet ; une
  manche jouée au clavier seul avec anneau de focus vérifié ; six tailles de 360 à 1920 px avec axe.

## Pourquoi pas autrement
- Une piste silencieuse **de durée nulle**, lue en boucle, fait tourner le lecteur à vide et fige la page : trouvé par
  le test téléphone, d'où le test qui vérifie que la piste a de vrais échantillons.
- Mesurer les effets par `getBoundingClientRect` donne des faux positifs : les animations commencent à une échelle
  différente ; on mesure la boîte de mise en page.

## Conséquences
- `MAX_ARC_OPPONENTS` passe de 4 à 3.
- Les boutons de l'accueil et du salon gardent le contour de focus global (ils le masquaient).
