import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { Backdrop } from './ui/backdrop';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet, Backdrop],
  template: '<app-backdrop /><router-outlet />',
})
export class App {}
