import { TestBed } from '@angular/core/testing';
import type { Emote } from '../../protocol/generated/protocol';
import { ReactionBubble } from './reaction-bubble';

function render(emote: Emote | null) {
  const fixture = TestBed.createComponent(ReactionBubble);
  fixture.componentRef.setInput('emote', emote);
  fixture.componentRef.setInput('nickname', 'Zoé');
  fixture.detectChanges();
  return fixture.nativeElement as HTMLElement;
}

describe('ReactionBubble', () => {
  it('shows the glyph and announces who reacted, in words', () => {
    const root = render('gg');

    expect(root.querySelector('.bubble')?.textContent).toContain('👏');
    expect(root.querySelector('.bubble')?.getAttribute('aria-hidden')).toBe('true');
    expect(root.querySelector('[role="status"]')?.textContent).toBe('Zoé : Bien joué');
  });

  it('shows nothing and announces nothing without a reaction', () => {
    const root = render(null);

    expect(root.querySelector('.bubble')).toBeNull();
    expect(root.querySelector('[role="status"]')?.textContent).toBe('');
  });
});
