// Mesure d'accessibilité avec axe (WCAG 2.2 AA) sur la page ouverte :
//   playwright-cli run-code --filename=scripts/axe-snippet.js     (après "npm start" et un serveur lancé)
// Renvoie la liste des violations ; [] quand tout est conforme.
async (page) => {
  await page.addScriptTag({ path: 'node_modules/axe-core/axe.min.js' });
  return await page.evaluate(async () => {
    const result = await window.axe.run(document, {
      runOnly: { type: 'tag', values: ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'] },
    });
    return result.violations.map((v) => ({
      id: v.id,
      impact: v.impact,
      nodes: v.nodes.slice(0, 4).map((n) => ({
        target: n.target.join(' '),
        summary: (n.any[0] ?? n.all[0] ?? n.none[0] ?? {}).message,
      })),
    }));
  });
};
