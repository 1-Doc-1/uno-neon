import { ApplicationConfig, provideBrowserGlobalErrorListeners } from '@angular/core';
import { provideRouter, TitleStrategy, withComponentInputBinding } from '@angular/router';
import { routes } from './app.routes';
import { GAME_TRANSPORT } from './core/game-transport';
import { PageTitleStrategy } from './core/page-title';
import { WebSocketTransport } from './core/web-socket-transport';

export const appConfig: ApplicationConfig = {
  providers: [
    provideBrowserGlobalErrorListeners(),
    provideRouter(routes, withComponentInputBinding()),
    { provide: TitleStrategy, useClass: PageTitleStrategy },
    {
      provide: GAME_TRANSPORT,
      useFactory: () =>
        new WebSocketTransport(
          `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`,
        ),
    },
  ],
};
