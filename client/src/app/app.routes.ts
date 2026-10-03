import { Routes } from '@angular/router';
import { DEV_ROUTES } from './dev/dev.routes';

export const routes: Routes = [
  { path: '', loadComponent: () => import('./features/home/home-page').then((m) => m.HomePage) },
  {
    path: 'r/:code',
    loadComponent: () => import('./features/room/room-page').then((m) => m.RoomPage),
  },
  // Vide dans le build de production (angular.json, fileReplacements)
  ...DEV_ROUTES,
  { path: '**', redirectTo: '' },
];
