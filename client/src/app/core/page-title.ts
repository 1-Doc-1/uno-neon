import { Service } from '@angular/core';
import { Title } from '@angular/platform-browser';
import { RouterStateSnapshot, TitleStrategy } from '@angular/router';
import { inject } from '@angular/core';
import { APP_NAME } from './app-name';

/** Le titre de l'onglet suit la page : « Comment jouer · UNO », et « UNO » seul quand la route n'a pas de titre. */
@Service()
export class PageTitleStrategy extends TitleStrategy {
  private readonly title = inject(Title);

  override updateTitle(snapshot: RouterStateSnapshot): void {
    const page = this.buildTitle(snapshot);
    this.title.setTitle(page ? `${page} · ${APP_NAME}` : APP_NAME);
  }
}
