import { TestBed } from '@angular/core/testing';
import { MAX_FAN_BACKS, OpponentFan } from './opponent-fan';

function render(count: number, maxBacks = MAX_FAN_BACKS) {
  const fixture = TestBed.createComponent(OpponentFan);
  fixture.componentRef.setInput('count', count);
  fixture.componentRef.setInput('maxBacks', maxBacks);
  fixture.detectChanges();
  return fixture.nativeElement as HTMLElement;
}

describe('OpponentFan', () => {
  it('draws one back per card, capped, with the surplus in a "+N" label', () => {
    const few = render(4);
    const many = render(15);

    expect(few.querySelectorAll('app-card-back')).toHaveLength(4);
    expect(few.querySelector('.more')).toBeNull();
    expect(many.querySelectorAll('app-card-back')).toHaveLength(MAX_FAN_BACKS);
    expect(many.querySelector('.more')?.textContent).toBe('+3');
  });

  it('keeps the fan nearly flat: a gentle step that shrinks as the fan grows', () => {
    const step = (count: number) => parseFloat(render(count).style.getPropertyValue('--step'));

    expect(step(2)).toBeLessThanOrEqual(4);
    expect(step(12)).toBeLessThan(step(4));
  });
});
