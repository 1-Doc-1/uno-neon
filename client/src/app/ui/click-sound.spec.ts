import { readdirSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { AudioService } from '../audio/audio.service';
import { ClickSound } from './click-sound';

@Component({ imports: [ClickSound], template: '<button type="button">Ok</button>' })
class Host {}

const appDir = resolve(process.cwd(), 'src/app');

function sourcesOf(dir: string): string[] {
  return readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
    const path = join(dir, entry.name);
    return entry.isDirectory() ? (entry.name === 'dev' ? [] : sourcesOf(path)) : [path];
  });
}

describe('ClickSound', () => {
  beforeEach(() => localStorage.clear());

  it('plays the interface click when a button is clicked', () => {
    const play = vi.spyOn(TestBed.inject(AudioService), 'play');
    const fixture = TestBed.createComponent(Host);
    fixture.detectChanges();

    (fixture.nativeElement as HTMLElement).querySelector('button')?.click();

    expect(play).toHaveBeenCalledWith('click');
  });

  it('is imported by every component whose template has a button', () => {
    const sources = sourcesOf(appDir).filter(
      (path) => /\.(ts|html)$/.test(path) && !path.endsWith('.spec.ts'),
    );
    const forgotten = sources
      .filter((path) => /<button\b/.test(readFileSync(path, 'utf8')))
      .map((path) => path.replace(/\.html$/, '.ts'))
      .filter((path) => !/(click-sound|ui[\\/]button)\.ts$/.test(path))
      .filter((component) => !readFileSync(component, 'utf8').includes('ClickSound'));

    expect(forgotten).toEqual([]);
  });
});
