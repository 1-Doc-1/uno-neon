import { Routes } from '@angular/router';

export const routes: Routes = [
  { path: '', loadComponent: () => import('./features/home/home-page').then((m) => m.HomePage) },
  {
    path: 'r/:code',
    loadComponent: () => import('./features/room/room-page').then((m) => m.RoomPage),
  },
  { path: 'dev', loadComponent: () => import('./dev/ui-preview').then((m) => m.UiPreview) },
  {
    path: 'dev/table',
    loadComponent: () => import('./dev/table-fixture').then((m) => m.TableFixture),
  },
  { path: '**', redirectTo: '' },
];
