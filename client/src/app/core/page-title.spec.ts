import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { Router, TitleStrategy, provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { APP_NAME } from './app-name';
import { PageTitleStrategy } from './page-title';

@Component({ template: '' })
class Blank {}

describe('PageTitleStrategy', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        provideRouter([
          { path: '', component: Blank },
          { path: 'comment-jouer', title: 'Comment jouer', component: Blank },
        ]),
        { provide: TitleStrategy, useClass: PageTitleStrategy },
      ],
    });
  });

  it('puts the page before the name of the game in the title of the tab', async () => {
    const harness = await RouterTestingHarness.create();

    await harness.navigateByUrl('/comment-jouer');

    expect(document.title).toBe(`Comment jouer · ${APP_NAME}`);
  });

  it('shows the name of the game alone on a page without a title, and follows the navigation', async () => {
    const harness = await RouterTestingHarness.create();
    await harness.navigateByUrl('/comment-jouer');

    await TestBed.inject(Router).navigateByUrl('/');

    expect(document.title).toBe(APP_NAME);
  });
});
