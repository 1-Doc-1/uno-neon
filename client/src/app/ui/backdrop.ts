import { Component } from '@angular/core';

/** Fond « Crépuscule » fixe, monté une seule fois à la racine. */
@Component({
  selector: 'app-backdrop',
  template: '<div class="backdrop" aria-hidden="true"></div>',
})
export class Backdrop {}
