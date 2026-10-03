import { TestBed } from '@angular/core/testing';
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
});
