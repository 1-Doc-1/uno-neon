import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { Backdrop } from './ui/backdrop';
import { ConnectionBanner } from './ui/connection-banner';
import { Toasts } from './ui/toasts';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet, Backdrop, Toasts, ConnectionBanner],
  template: '<app-backdrop /><router-outlet /><app-connection-banner /><app-toasts />',
})
export class App {}
