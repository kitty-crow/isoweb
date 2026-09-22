const routes = [
  {
    name: 'Level Builder',
    html: 'site/level-builder/index.html',
    bundle: 'site/assets/level-builder.js',
    marker: 'Level Builder'
  },
  {
    name: 'World Editor',
    html: 'site/world-editor/index.html',
    bundle: 'site/assets/world-editor.js',
    marker: 'World Editor'
  }
];

for (const route of routes) {
  const htmlFile = Bun.file(route.html);
  if (!await htmlFile.exists()) throw new Error(`${route.name} route was not built at ${route.html}`);
  const html = await htmlFile.text();
  if (!html.includes(route.marker) || !html.includes('../assets/') && !html.includes('assets/')) {
    throw new Error(`${route.name} route does not contain its expected editor shell`);
  }

  const bundle = Bun.file(route.bundle);
  if (!await bundle.exists() || bundle.size < 1000) {
    throw new Error(`${route.name} browser bundle is missing or unexpectedly small`);
  }
}

const root = await Bun.file('site/index.html').text();
if (!root.includes('modeLoader.js')) {
  throw new Error('Normal runtime route was changed while adding editor routes');
}

console.log('Editor route smoke passed: /level-builder/ and /world-editor/ build independently while / remains the runtime app.');
