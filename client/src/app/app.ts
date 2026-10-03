import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { ConnectionBanner } from './ui/connection-banner';
import { Toasts } from './ui/toasts';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet, Toasts, ConnectionBanner],
  template: '<router-outlet /><app-connection-banner /><app-toasts />',
})
export class App {}
