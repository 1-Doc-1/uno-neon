import { Routes } from '@angular/router';
import { DEV_ROUTES } from './dev/dev.routes';

export const routes: Routes = [
  { path: '', loadComponent: () => import('./features/home/home-page').then((m) => m.HomePage) },
  {
    path: 'bots',
    title: 'Jouer contre des bots',
    loadComponent: () => import('./features/bots/bot-game-page').then((m) => m.BotGamePage),
  },
  {
    path: 'comment-jouer',
    title: 'Comment jouer',
    loadComponent: () =>
      import('./features/how-to-play/how-to-play-page').then((m) => m.HowToPlayPage),
  },
  {
    path: 'r/:code',
    title: 'Salon',
    loadComponent: () => import('./features/room/room-page').then((m) => m.RoomPage),
  },
  // Vide dans le build de production (angular.json, fileReplacements)
  ...DEV_ROUTES,
  { path: '**', redirectTo: '' },
];
