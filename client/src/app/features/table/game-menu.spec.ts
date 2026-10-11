import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it } from 'vitest';
import { GameMenu } from './game-menu';

describe('GameMenu', () => {
  beforeEach(() => {
    HTMLDialogElement.prototype.showModal = function showModal(): void {
      this.setAttribute('open', '');
    };
  });

  function render(isHost: boolean) {
    const fixture = TestBed.createComponent(GameMenu);
    fixture.componentRef.setInput('isHost', isHost);
    const events: string[] = [];
    fixture.componentInstance.backToLobby.subscribe(() => events.push('lobby'));
    fixture.componentInstance.left.subscribe(() => events.push('left'));
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;
    const button = (text: string) =>
      [...host.querySelectorAll<HTMLButtonElement>('button')].find((b) =>
        b.textContent?.includes(text),
      );
    return { fixture, host, button, events };
  }

  it('lets the host go back to the lobby only after a confirmation that says the match is lost', () => {
    const { fixture, host, button, events } = render(true);

    button('Menu')?.click();
    fixture.detectChanges();
    button('Retour au salon')?.click();
    fixture.detectChanges();

    expect(host.textContent).toContain('Abandonner la partie ?');
    expect(host.textContent).toContain('sans score');
    expect(events).toEqual([]);
    button('Retour au salon')?.click();
    expect(events).toEqual(['lobby']);
  });

  it('gives the others only the way out', () => {
    const { fixture, button, events } = render(false);

    button('Menu')?.click();
    fixture.detectChanges();

    expect(button('Retour au salon')).toBeUndefined();
    button('Quitter la partie')?.click();
    expect(events).toEqual(['left']);
  });
});
