import { Routes } from '@angular/router';

/**
 * Les pages de développement (`/dev`, `/dev/table`) : cartes, composants et scénarios de table écrits à la main.
 * Le build de production remplace ce fichier par `dev.routes.prod.ts` (voir `fileReplacements` dans angular.json) :
 * ni les routes, ni les fixtures, ni les composants de ces pages n'entrent dans le bundle publié.
 */
export const DEV_ROUTES: Routes = [
  { path: 'dev', loadComponent: () => import('./ui-preview').then((m) => m.UiPreview) },
  { path: 'dev/table', loadComponent: () => import('./table-fixture').then((m) => m.TableFixture) },
];
