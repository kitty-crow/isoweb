import { createRequire } from 'node:module';
import { readFile } from 'node:fs/promises';
import { extname, normalize } from 'node:path';

import { CompiledPackageWriter } from '../web/src/world/CompiledPackageWriter';
import { PackageWriter } from '../web/src/world/PackageWriter';
import { WorldCompiler } from '../web/src/world/WorldCompiler';
import type { LevelDocument, WorldDocument } from '../web/src/world/documents';
import { validateLevelDocument, validateWorldDocument } from '../web/src/world/validation';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const clone = <T>(value: T): T => JSON.parse(JSON.stringify(value));

const originalWorld = validateWorldDocument(
  JSON.parse(await readFile('web/worlds/demo/world.json', 'utf8')) as WorldDocument
);
const originalMiddle = validateLevelDocument(
  JSON.parse(await readFile('web/worlds/demo/levels/middle.json', 'utf8')) as LevelDocument
);

const level = clone(originalMiddle);
level.assets = {
  'probe.png': { source: 'probe.png', mediaType: 'image/png' }
};
const character = level.entities.find(entity => entity.id === 'demo-character');
if (!character || !('character' in character.components)) throw new Error('Demo Character fixture missing');
character.components.character.sprites = {
  still: {
    front: {
      resource: 'probe.png',
      frameCount: 1,
      columns: 1,
      rows: 1,
      fps: 1,
      worldWidth: 0.7,
      worldHeight: 1.65,
      loop: true
    }
  }
};
validateLevelDocument(level);

const world = clone(originalWorld);
world.levels = [{ id: 'middle', path: 'levels/middle.json' }];
world.settings.defaultLevel = 'middle';
world.connectors = [];
world.behaviours = [];
world.assets = {};
validateWorldDocument(world);

const probePng = Uint8Array.from(
  Buffer.from(
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=',
    'base64'
  )
);
const levelAssets = new Map([
  ['probe.png', { bytes: probePng, mediaType: 'image/png' }]
]);

const compiler = new WorldCompiler();
const compiledLevel = compiler.compileLevel(level);
const compiledWriter = new CompiledPackageWriter();
const compiledLevelBytes = compiledWriter.writeLevelBytes(compiledLevel, levelAssets);
const compiledWorld = compiler.compileWorld(
  world,
  new Map([['middle', 'levels/middle.isolevel']])
);
const compiledWorldBytes = compiledWriter.writeWorldBytes(
  compiledWorld,
  [compiledLevel],
  new Map([['middle', compiledLevelBytes]])
);

// Build the source form too and make sure it is browser-loadable via the same
// package asset resolution path.
const sourceWorldBytes = new PackageWriter().writeWorldBytes(
  world,
  [level],
  levelAssets
);

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.wasm': 'application/wasm',
  '.isoworld': 'application/octet-stream'
};

let forbiddenAssetRequests = 0;
const server = Bun.serve({
  port: 0,
  async fetch(request) {
    const url = new URL(request.url);
    if (url.pathname.endsWith('/probe.png')) {
      forbiddenAssetRequests += 1;
      return new Response('External asset request forbidden', { status: 418 });
    }
    if (url.pathname === '/embedded-compiled.isoworld') {
      return new Response(compiledWorldBytes, {
        headers: { 'Content-Type': 'application/octet-stream' }
      });
    }
    if (url.pathname === '/embedded-source.isoworld') {
      return new Response(sourceWorldBytes, {
        headers: { 'Content-Type': 'application/octet-stream' }
      });
    }

    let pathname = decodeURIComponent(url.pathname);
    if (pathname === '/') pathname = '/index.html';
    if (pathname === '/favicon.ico') return new Response(null, { status: 204 });
    const relative = normalize(pathname).replace(/^[/\\]+/, '');
    if (relative.startsWith('..')) return new Response('Forbidden', { status: 403 });
    const file = Bun.file(`site/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, {
      headers: {
        'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream'
      }
    });
  }
});

const browser = await chromium.launch({ headless: true });

async function prove(path: string): Promise<void> {
  const page = await browser.newPage({ viewport: { width: 640, height: 480 } });
  const errors: string[] = [];
  page.on('pageerror', (error: Error) => errors.push(error.stack || error.message));
  page.on('console', (message: any) => {
    if (message.type() === 'error') errors.push(message.text());
  });

  try {
    console.log('[embedded-assets] loading ' + path);
    await page.goto(
      `http://127.0.0.1:${server.port}/?presentation=canvas2d&world=${encodeURIComponent(path)}`,
      { waitUntil: 'domcontentloaded' }
    );
    try {
      await page.waitForFunction(
        () => document.documentElement.classList.contains('world-ready'),
        undefined,
        { timeout: 45_000 }
      );
    } catch (error) {
      const state = await page.evaluate(() => ({
        ready: document.documentElement.classList.contains('world-ready'),
        loading: document.getElementById('loading')?.textContent ?? null,
        href: location.href
      })).catch(() => null);
      throw new Error(
        path + ' did not become world-ready. state=' + JSON.stringify(state) +
        ' errors=' + JSON.stringify(errors) +
        ' forbiddenAssetRequests=' + forbiddenAssetRequests +
        ' cause=' + String(error)
      );
    }

    const state = await page.evaluate(() => ({
      characters: (globalThis as any).Module._isoweb_character_count(),
      worldReady: document.documentElement.classList.contains('world-ready')
    }));
    if (!state.worldReady || state.characters !== 1) {
      throw new Error(`Embedded asset world failed to instantiate: ${JSON.stringify(state)}`);
    }
    if (errors.length) throw new Error(errors.join('\n\n'));
  } finally {
    await page.close();
  }
}

try {
  await prove('embedded-compiled.isoworld');
  await prove('embedded-source.isoworld');

  if (forbiddenAssetRequests !== 0) {
    throw new Error(
      `Runtime attempted ${forbiddenAssetRequests} external asset request(s) for probe.png`
    );
  }

  console.log(
    'Embedded asset browser smoke passed: compiled and source isoworld packages decoded sprite bytes from their own archives with zero external sprite requests.'
  );
} finally {
  await browser.close();
  server.stop(true);
}
