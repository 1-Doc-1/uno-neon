import { Routes } from '@angular/router';

export const routes: Routes = [
  { path: 'dev', loadComponent: () => import('./dev/ui-preview').then((m) => m.UiPreview) },
];
