import type { Player } from './player.ts';

/** Ce qui ne doit jamais arriver : la page défile, ou un siège sort de l'écran. */
export async function layoutProblems(player: Player): Promise<string[]> {
  return player.page.evaluate(() => {
    const problems: string[] = [];
    const root = document.documentElement;
    if (root.scrollWidth > root.clientWidth) {
      problems.push(`défilement horizontal : ${root.scrollWidth} > ${root.clientWidth}`);
    }
    if (root.scrollHeight > window.innerHeight + 1) {
      const lowest = [...document.body.querySelectorAll<HTMLElement>('*')]
        .filter((element) => element.getBoundingClientRect().bottom > window.innerHeight + 1)
        .filter((element) => !(element instanceof SVGElement))
        .slice(-3)
        .map(
          (element) =>
            `${element.tagName.toLowerCase()}.${element.className} ${Math.round(element.getBoundingClientRect().bottom)}`,
        );
      problems.push(
        `défilement vertical : ${root.scrollHeight} > ${window.innerHeight} (${lowest.join(', ')})`,
      );
    }
    // La bande défilante (au-delà de trois adversaires en portrait étroit) sort de l'écran par conception
    for (const seat of document.querySelectorAll('.seats:not(.strip) app-opponent-seat')) {
      const box = seat.getBoundingClientRect();
      if (
        box.left < -1 ||
        box.top < -1 ||
        box.right > root.clientWidth + 1 ||
        box.bottom > window.innerHeight + 1
      ) {
        problems.push(
          `siège hors écran : ${Math.round(box.left)},${Math.round(box.top)} → ${Math.round(box.right)},${Math.round(box.bottom)}`,
        );
      }
    }
    return problems;
  });
}
