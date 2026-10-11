import { TestBed } from '@angular/core/testing';
import { describe, expect, it } from 'vitest';
import { ChatDisplayLine, ChatPanel } from './chat-panel';

const line = (id: number, nickname: string, text: string, mine = false): ChatDisplayLine => ({
  id,
  nickname,
  avatarId: id % 12,
  text,
  mine,
});

function render(lines: ChatDisplayLine[], open: boolean, unread = 0) {
  const fixture = TestBed.createComponent(ChatPanel);
  fixture.componentRef.setInput('lines', lines);
  fixture.componentRef.setInput('open', open);
  fixture.componentRef.setInput('unread', unread);
  const sent: string[] = [];
  const toggled: boolean[] = [];
  fixture.componentInstance.sent.subscribe((text) => sent.push(text));
  fixture.componentInstance.toggled.subscribe((value) => toggled.push(value));
  fixture.detectChanges();
  return { fixture, host: fixture.nativeElement as HTMLElement, sent, toggled };
}

describe('ChatPanel', () => {
  it('is folded by default: a button with the number of unread messages', () => {
    const { host, toggled } = render([], false, 3);

    const toggle = host.querySelector<HTMLButtonElement>('button.toggle');
    expect(toggle?.getAttribute('aria-expanded')).toBe('false');
    expect(host.querySelector('.unread')?.textContent?.trim()).toBe('3');
    expect(host.querySelector('input')).toBeNull();

    toggle?.click();
    expect(toggled).toEqual([true]);
  });

  it('shows no counter when nothing is unread', () => {
    const { host } = render([], false, 0);

    expect(host.querySelector('.unread')).toBeNull();
  });

  it('shows what was said as plain text, never as markup', () => {
    const { host } = render(
      [line(1, 'Loïc', '<img src=x onerror=alert(1)> <b>gras</b>'), line(2, 'Léa', 'ok', true)],
      true,
    );

    const first = host.querySelectorAll('li')[0];
    expect(first.textContent).toContain('<img src=x onerror=alert(1)> <b>gras</b>');
    expect(host.querySelector('li img')).toBeNull();
    expect(host.querySelector('li b')).toBeNull();
    expect(host.querySelectorAll('li')[1].classList.contains('mine')).toBe(true);
  });

  it('says so when nobody spoke yet', () => {
    const { host } = render([], true);

    expect(host.querySelector('.empty')?.textContent).toContain('Aucun message');
  });

  it('sends the trimmed text on submit, then clears the field', () => {
    const { fixture, host, sent } = render([], true);
    const input = host.querySelector<HTMLInputElement>('input') as HTMLInputElement;

    input.value = '  salut  ';
    input.dispatchEvent(new Event('input'));
    fixture.detectChanges();
    host.querySelector('form')?.dispatchEvent(new Event('submit', { cancelable: true }));
    fixture.detectChanges();

    expect(sent).toEqual(['salut']);
    expect(input.value).toBe('');
  });

  it('does not send an empty message, and limits the field to 200 characters', () => {
    const { fixture, host, sent } = render([], true);
    const input = host.querySelector<HTMLInputElement>('input') as HTMLInputElement;

    input.value = '   ';
    input.dispatchEvent(new Event('input'));
    fixture.detectChanges();

    expect(host.querySelector<HTMLButtonElement>('form button')?.disabled).toBe(true);
    host.querySelector('form')?.dispatchEvent(new Event('submit', { cancelable: true }));
    expect(sent).toEqual([]);
    expect(input.maxLength).toBe(200);
  });
});
