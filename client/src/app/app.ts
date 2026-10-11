import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { ConnectionBanner } from './ui/connection-banner';
import { Intro } from './ui/intro';
import { SessionBusy } from './ui/session-busy';
import { Toasts } from './ui/toasts';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet, Toasts, ConnectionBanner, Intro, SessionBusy],
  template:
    '<router-outlet /><app-connection-banner /><app-toasts /><app-session-busy /><app-intro />',
})
export class App {}
