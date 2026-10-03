import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';

import { GAME_TRANSPORT } from './core/game-transport';
import { FakeTransport } from './testing/fake-transport';
import { App } from './app';

describe('App', () => {
  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [App],
      providers: [provideRouter([]), { provide: GAME_TRANSPORT, useValue: new FakeTransport() }],
    }).compileComponents();
  });

  it('creates the root component', () => {
    const fixture = TestBed.createComponent(App);

    expect(fixture.componentInstance).toBeInstanceOf(App);
  });

  it('hosts the router outlet that displays each page', async () => {
    const fixture = TestBed.createComponent(App);
    await fixture.whenStable();
    const host = fixture.nativeElement as HTMLElement;

    expect(host.querySelector('router-outlet')).not.toBeNull();
  });
});
