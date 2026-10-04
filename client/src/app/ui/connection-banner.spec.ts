import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { GAME_TRANSPORT } from '../core/game-transport';
import { FakeTransport } from '../testing/fake-transport';
import { ConnectionBanner } from './connection-banner';

@Component({ template: '' })
class Blank {}

describe('ConnectionBanner', () => {
  async function setup(url: string) {
    const transport = new FakeTransport();
    TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: 'dev/table', component: Blank },
          { path: 'r/:code', component: Blank },
        ]),
        { provide: GAME_TRANSPORT, useValue: transport },
      ],
    });
    await TestBed.inject(Router).navigateByUrl(url);
    const fixture = TestBed.createComponent(ConnectionBanner);
    transport.statusState.set('reconnecting'); // le store a ouvert la connexion : on la coupe
    fixture.detectChanges();
    return { fixture, host: fixture.nativeElement as HTMLElement };
  }

  it('warns that the connection is lost on a game page', async () => {
    const { host } = await setup('/r/ABCD');

    expect(host.textContent).toContain('Connexion perdue');
  });

  it('stays silent on the /dev pages, which have no WebSocket', async () => {
    const { host } = await setup('/dev/table?scenario=animations');

    expect(host.textContent).not.toContain('Connexion perdue');
  });
});
