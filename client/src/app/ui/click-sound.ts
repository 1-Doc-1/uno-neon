import { Directive, inject } from '@angular/core';
import { AudioService } from '../audio/audio.service';

/**
 * Le clic d'interface (SPEC §13.3) : à importer dans tout composant dont le gabarit contient un `<button>` ; elle
 * s'applique d'elle-même à chacun. Le test `click-sound.spec.ts` vérifie qu'aucun bouton n'est oublié.
 */
@Directive({
  // Posée sur tous les <button> natifs, sans attribut à écrire : l'oubli est justement ce qu'on veut éviter
  // eslint-disable-next-line @angular-eslint/directive-selector
  selector: 'button',
  host: { '(click)': 'audio.play("click")' },
})
export class ClickSound {
  protected readonly audio = inject(AudioService);
}
