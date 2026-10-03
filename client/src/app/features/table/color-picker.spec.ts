import { TestBed } from '@angular/core/testing';
import type { Color } from '../../protocol/generated/protocol';
import { ColorPicker } from './color-picker';

describe('ColorPicker', () => {
  beforeEach(() => {
    // jsdom n'implémente pas <dialog>.showModal()
    HTMLDialogElement.prototype.showModal = function showModal(): void {
      this.setAttribute('open', '');
    };
  });

  it('picks a colour with the 1 to 4 keys or a click on its tile', async () => {
    const fixture = TestBed.createComponent(ColorPicker);
    const picked: Color[] = [];
    fixture.componentInstance.picked.subscribe((color) => picked.push(color));
    await fixture.whenStable();

    document.dispatchEvent(new KeyboardEvent('keydown', { key: '3' }));
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '9' }));
    (fixture.nativeElement as HTMLElement)
      .querySelector<HTMLButtonElement>('.tile.tint-blue')
      ?.click();

    expect(picked).toEqual(['green', 'blue']);
  });
});
