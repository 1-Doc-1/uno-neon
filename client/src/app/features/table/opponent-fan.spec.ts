import { TestBed } from '@angular/core/testing';
import { fanStepRatio, fanWidthRatio, MAX_FAN_BACKS, OpponentFan } from './opponent-fan';

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
    const step = (count: number) => parseFloat(render(count).style.getPropertyValue('--ratio'));

    expect(step(2)).toBeLessThanOrEqual(0.42);
    expect(step(15)).toBeLessThan(step(4));
    expect(fanWidthRatio(15)).toBeLessThanOrEqual(3.6 + 1e-9);
    expect(fanWidthRatio(MAX_FAN_BACKS)).toBeGreaterThan(fanWidthRatio(5));
  });

  it('draws the backs side by side, straight: no rotation, no turn around any axis', () => {
    const fixture = TestBed.createComponent(OpponentFan);
    fixture.componentRef.setInput('count', 5);
    fixture.detectChanges();
    const host = fixture.nativeElement as HTMLElement;

    expect(host.style.getPropertyValue('--yaw')).toBe('');
    expect(host.style.getPropertyValue('--step')).toBe('');
    expect(host.style.getPropertyValue('--ratio')).toBe(String(fanStepRatio(5)));
    expect(host.querySelectorAll('app-card-back')).toHaveLength(5);
  });

  it('keeps one card as wide as a single back, whatever the count', () => {
    expect(fanWidthRatio(1)).toBe(1);
    expect(fanWidthRatio(0)).toBe(1);
  });
});
