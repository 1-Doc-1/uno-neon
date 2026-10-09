import { TestBed } from '@angular/core/testing';
import type { Card } from '../protocol/generated/protocol';
import { CardFace } from './card';
import { cardLabel } from './color-meta';

describe('CardFace', () => {
  it('describes a card for screen readers', () => {
    expect(cardLabel({ id: 1, color: 'blue', rank: 'skip' })).toBe('Passe-tour bleu');
    expect(cardLabel({ id: 2, color: null, rank: 'wildDrawFour' })).toBe('Joker plus quatre');
  });

  it('shows the shape of its colour in both corners and the chosen colour of a wild', () => {
    const fixture = TestBed.createComponent(CardFace);
    fixture.componentRef.setInput('card', { id: 3, color: 'red', rank: '7' });
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;

    expect(host.getAttribute('aria-label')).toBe('7 rouge');
    expect(host.querySelectorAll('.mark polygon')).toHaveLength(2);

    fixture.componentRef.setInput('card', { id: 4, color: null, rank: 'wild' });
    fixture.componentRef.setInput('chosenColor', 'green');
    fixture.detectChanges();

    expect(host.querySelector('.chosen')).not.toBeNull();
  });

  describe('the Wild Draw Five, a golden joker', () => {
    const plusFive: Card = { id: 5, color: null, rank: 'wildDrawFive' };

    function render(card: Card = plusFive) {
      const fixture = TestBed.createComponent(CardFace);
      fixture.componentRef.setInput('card', card);
      fixture.detectChanges();
      return fixture.nativeElement as HTMLElement;
    }

    it('is named for screen readers', () => {
      expect(cardLabel(plusFive)).toBe('Joker plus cinq');
      expect(render().getAttribute('aria-label')).toBe('Joker plus cinq');
    });

    it('is gold, with a big readable "+5", its colour wheel and a glint that passes over it', () => {
      const host = render();

      expect(host.classList.contains('tint-gold')).toBe(true);
      expect(host.querySelector('.gold-value')?.textContent?.trim()).toBe('+5');
      expect(host.querySelector('.ring')).not.toBeNull();
      expect(host.querySelector('.glint')).not.toBeNull();
      const fill = host.querySelector<SVGRectElement>('.body')?.style.fill ?? '';
      expect(fill).toMatch(/^url\(["']?#gold-\d+-metal["']?\)$/);
    });

    it('gives every golden card its own gradient, so that one leaving the table never blanks another', () => {
      const first = render();
      const second = render({ ...plusFive, id: 6 });

      const ids = [first, second].map((host) => host.querySelector('linearGradient')?.id);
      expect(ids[0]).not.toBe(ids[1]);
    });

    it('leaves every other card without the gold', () => {
      const host = render({ id: 1, color: null, rank: 'wild' });

      expect(host.classList.contains('tint-gold')).toBe(false);
      expect(host.querySelector('.gold-value')).toBeNull();
    });
  });
});
