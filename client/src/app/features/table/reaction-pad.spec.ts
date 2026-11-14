import { TestBed } from '@angular/core/testing';
import type { Emote } from '../../protocol/generated/protocol';
import { ReactionPad } from './reaction-pad';

function render(cooling = false) {
  const fixture = TestBed.createComponent(ReactionPad);
  fixture.componentRef.setInput('cooling', cooling);
  fixture.detectChanges();
  const picked: Emote[] = [];
  fixture.componentInstance.picked.subscribe((emote) => picked.push(emote));
  const root = fixture.nativeElement as HTMLElement;
  const toggle = () => root.querySelector<HTMLButtonElement>('button.toggle');
  const choices = () => Array.from(root.querySelectorAll<HTMLButtonElement>('button.choice'));
  const open = () => {
    toggle()?.click();
    fixture.detectChanges();
  };
  return { fixture, root, picked, toggle, choices, open };
}

describe('ReactionPad', () => {
  it('shows nothing but its button until it is opened, then the six fixed reactions', () => {
    const pad = render();
    expect(pad.choices()).toHaveLength(0);
    expect(pad.toggle()?.getAttribute('aria-expanded')).toBe('false');

    pad.open();

    expect(pad.toggle()?.getAttribute('aria-expanded')).toBe('true');
    expect(pad.choices().map((button) => button.getAttribute('aria-label'))).toEqual([
      'Bien joué',
      'Wow',
      'Mort de rire',
      'Aïe',
      'Hmm',
      'En feu',
    ]);
  });

  it('sends the chosen reaction with one click, then folds back', () => {
    const pad = render();
    pad.open();

    pad.choices()[5].click();
    pad.fixture.detectChanges();

    expect(pad.picked).toEqual(['fire']);
    expect(pad.choices()).toHaveLength(0);
  });

  it('greys the choices out while the server delay runs', () => {
    const pad = render(true);
    pad.open();

    expect(pad.choices().every((button) => button.disabled)).toBe(true);
    pad.choices()[0].click();
    expect(pad.picked).toEqual([]);
  });

  it('folds back when the focus goes elsewhere, but not when it moves between its own buttons', () => {
    const pad = render();
    pad.open();

    pad
      .choices()[0]
      .dispatchEvent(
        new FocusEvent('focusout', { bubbles: true, relatedTarget: pad.choices()[1] }),
      );
    pad.fixture.detectChanges();
    expect(pad.choices()).toHaveLength(6);

    pad
      .choices()[0]
      .dispatchEvent(new FocusEvent('focusout', { bubbles: true, relatedTarget: document.body }));
    pad.fixture.detectChanges();
    expect(pad.choices()).toHaveLength(0);
  });

  it('folds back on Escape', () => {
    const pad = render();
    pad.open();

    pad.root.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    pad.fixture.detectChanges();

    expect(pad.choices()).toHaveLength(0);
  });
});
