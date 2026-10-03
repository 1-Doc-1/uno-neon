import { TestBed } from '@angular/core/testing';
import { TableCenter } from './table-center';

function render(currentColor: 'red' | null, direction: 'clockwise' | 'counterClockwise') {
  const fixture = TestBed.createComponent(TableCenter);
  fixture.componentRef.setInput('currentColor', currentColor);
  fixture.componentRef.setInput('direction', direction);
  fixture.detectChanges();
  return fixture.nativeElement as HTMLElement;
}

describe('TableCenter', () => {
  it('takes the active colour and runs the glow the other way when the direction reverses', () => {
    const clockwise = render('red', 'clockwise');
    const reversed = render('red', 'counterClockwise');

    expect(clockwise.classList.contains('tint-red')).toBe(true);
    expect(clockwise.classList.contains('reverse')).toBe(false);
    expect(reversed.classList.contains('reverse')).toBe(true);
  });

  it('stays neutral before any colour is active', () => {
    expect(render(null, 'clockwise').classList.contains('tint-wild')).toBe(true);
  });
});
