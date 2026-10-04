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
  it('draws one back per card, capped at 15, and leaves the exact count to the seat badge', () => {
    const few = render(4);
    const many = render(22);

    expect(few.querySelectorAll('app-card-back')).toHaveLength(4);
    expect(many.querySelectorAll('app-card-back')).toHaveLength(15);
    expect(MAX_FAN_BACKS).toBe(15);
    expect(many.querySelector('.more')).toBeNull();
  });

  it('spreads the cards less and less as the hand grows, so that it never gets wider than a few cards', () => {
    const step = (count: number) => parseFloat(render(count).style.getPropertyValue('--step'));

    expect(step(2)).toBeLessThanOrEqual(6);
    expect(step(15)).toBeLessThan(step(4));
  });

  it('turns the hand around the vertical axis only, never flat on the table', () => {
    const fixture = TestBed.createComponent(OpponentFan);
    fixture.componentRef.setInput('count', 5);
    fixture.componentRef.setInput('yaw', -24);
    fixture.detectChanges();

    expect((fixture.nativeElement as HTMLElement).style.getPropertyValue('--yaw')).toBe('-24deg');
  });
});
