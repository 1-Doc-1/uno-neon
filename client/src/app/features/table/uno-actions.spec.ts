import { TestBed } from '@angular/core/testing';
import { CatchButton, UnoActions } from './uno-actions';

function render(catchButtons: CatchButton[]) {
  const fixture = TestBed.createComponent(UnoActions);
  fixture.componentRef.setInput('catchButtons', catchButtons);
  fixture.componentRef.setInput('canCallUno', false);
  fixture.componentRef.setInput('mustDeclareUno', false);
  fixture.detectChanges();
  const caught: string[] = [];
  fixture.componentInstance.caught.subscribe((id) => caught.push(id));
  const buttons = Array.from(
    (fixture.nativeElement as HTMLElement).querySelectorAll<HTMLButtonElement>('button.catch'),
  );
  return { buttons, caught };
}

describe('UnoActions', () => {
  it('ignores a click on a counter-UNO button still in its grace period, then lets it through', () => {
    const grace = render([{ playerId: 'loic', nickname: 'Loïc', inGrace: true, secondsLeft: 2 }]);
    grace.buttons[0].click();
    const open = render([{ playerId: 'loic', nickname: 'Loïc', inGrace: false, secondsLeft: 9 }]);
    open.buttons[0].click();

    expect(grace.buttons[0].getAttribute('aria-disabled')).toBe('true');
    expect(grace.caught).toEqual([]);
    expect(open.caught).toEqual(['loic']);
  });
});
