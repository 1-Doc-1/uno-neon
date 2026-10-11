import { TestBed } from '@angular/core/testing';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { Color } from '../../protocol/generated/protocol';
import { CHOSEN_MS, ColorPicker } from './color-picker';

describe('ColorPicker', () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => vi.useRealTimers());

  async function render(cancellable = true) {
    const fixture = TestBed.createComponent(ColorPicker);
    fixture.componentRef.setInput('cancellable', cancellable);
    const picked: Color[] = [];
    let cancelled = 0;
    fixture.componentInstance.picked.subscribe((color) => picked.push(color));
    fixture.componentInstance.cancelled.subscribe(() => cancelled++);
    fixture.detectChanges();
    await fixture.whenStable();
    const host = fixture.nativeElement as HTMLElement;
    return { fixture, host, picked, cancelled: () => cancelled };
  }

  it('is one wheel of four quarter discs, a button each, named by colour', async () => {
    const { host } = await render();

    const wedges = [...host.querySelectorAll<HTMLButtonElement>('.wedge')];
    expect(wedges.map((w) => w.getAttribute('aria-label'))).toEqual([
      'Rouge',
      'Jaune',
      'Vert',
      'Bleu',
    ]);
    expect(host.querySelector('[role="dialog"]')?.getAttribute('aria-label')).toBe(
      'Choisir une couleur',
    );
    expect(wedges[0].classList.contains('q0')).toBe(true);
  });

  it('picks a colour with the 1 to 4 keys, and only once', async () => {
    const { picked } = await render();

    document.dispatchEvent(new KeyboardEvent('keydown', { key: '3' }));
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '9' }));
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '1' }));
    vi.advanceTimersByTime(CHOSEN_MS + 1);

    expect(picked).toEqual(['green']);
  });

  it('makes the chosen colour grow with sparks, fades the others, then sends the choice', async () => {
    const { fixture, host, picked } = await render();

    host.querySelector<HTMLButtonElement>('.wedge.tint-blue')?.click();
    fixture.detectChanges();

    expect(host.querySelector('.wedge.tint-blue')?.classList.contains('chosen')).toBe(true);
    expect(host.querySelectorAll('.wedge.faded')).toHaveLength(3);
    expect(host.querySelectorAll('.sparks i').length).toBeGreaterThan(8);
    expect(picked).toEqual([]);
    vi.advanceTimersByTime(CHOSEN_MS + 1);
    expect(picked).toEqual(['blue']);
  });

  it('cancels with Escape or a click outside the wheel, only when allowed', async () => {
    const free = await render(true);
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    free.host.querySelector<HTMLElement>('.backdrop')?.click();
    expect(free.cancelled()).toBe(2);

    const forced = await render(false);
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    forced.host.querySelector<HTMLElement>('.backdrop')?.click();
    expect(forced.cancelled()).toBe(0);
    expect(forced.host.querySelector('.cancel')).toBeNull();
  });

  it('puts the focus on the first colour so that the keyboard can go round the wheel', async () => {
    const { host } = await render();

    expect(document.activeElement).toBe(host.querySelector('.wedge'));
  });
});
