import { TestBed } from '@angular/core/testing';
import { describe, expect, it } from 'vitest';
import { AvatarPicker } from './avatar-picker';

function render(selected: number) {
  const fixture = TestBed.createComponent(AvatarPicker);
  fixture.componentRef.setInput('selected', selected);
  const picked: number[] = [];
  fixture.componentInstance.picked.subscribe((id) => picked.push(id));
  fixture.detectChanges();
  const radios = [
    ...(fixture.nativeElement as HTMLElement).querySelectorAll<HTMLElement>('[role="radio"]'),
  ];
  return { fixture, radios, picked };
}

describe('AvatarPicker', () => {
  it('offers the 12 avatars as one radio group, with only the chosen one checked', () => {
    const { radios } = render(4);

    expect(radios).toHaveLength(12);
    expect(radios.filter((r) => r.getAttribute('aria-checked') === 'true')).toEqual([radios[4]]);
    expect(radios.map((r) => r.getAttribute('aria-label'))).toContain('Fantôme');
  });

  it('tells which avatar was clicked', () => {
    const { radios, picked } = render(0);

    radios[7].click();

    expect(picked).toEqual([7]);
  });

  it('moves with the arrow keys, around the ends too', () => {
    const { radios, picked } = render(11);

    radios[11].dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
    radios[11].dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowLeft', bubbles: true }));
    radios[11].dispatchEvent(new KeyboardEvent('keydown', { key: 'Home', bubbles: true }));

    expect(picked).toEqual([0, 10, 0]);
  });
});
