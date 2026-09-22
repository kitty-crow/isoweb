import { createRequire } from 'node:module';
import { extname, normalize } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.wasm': 'application/wasm',
  '.isoworld': 'application/octet-stream'
};

const server = Bun.serve({
  port: 0,
  async fetch(request) {
    const url = new URL(request.url);
    let pathname = decodeURIComponent(url.pathname);
    if (pathname === '/') pathname = '/index.html';
    if (pathname === '/favicon.ico') return new Response(null, { status: 204 });
    const relative = normalize(pathname).replace(/^[/\\]+/, '');
    if (relative.startsWith('..')) return new Response('Forbidden', { status: 403 });
    const file = Bun.file(`site/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, { headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' } });
  }
});

const browser = await chromium.launch({ headless: true });
try {
  const page = await browser.newPage({ viewport: { width: 640, height: 480 } });
  await page.goto(`http://127.0.0.1:${server.port}/?webgl=0&obstacles=1`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('world-ready') &&
      (globalThis as any).Module?._isoweb_obstacles_enabled?.() === 1,
    undefined,
    { timeout: 45_000 }
  );

  const initial = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const position = (id: string) => ({
      x: module.ccall('isoweb_character_position_x', 'number', ['string'], [id]),
      y: module.ccall('isoweb_character_position_y', 'number', ['string'], [id]),
      z: module.ccall('isoweb_character_position_z', 'number', ['string'], [id])
    });
    return {
      characterCount: module._isoweb_character_count(),
      left: position('demo-obstacle-barrier-left'),
      right: position('demo-obstacle-barrier-right'),
      guillotine: position('demo-obstacle-guillotine'),
      bladeA: position('demo-obstacle-blade-a'),
      bladeB: position('demo-obstacle-blade-b')
    };
  });

  if (initial.characterCount !== 1) {
    throw new Error(`Dynamic bodies leaked into Character count: ${initial.characterCount}`);
  }
  for (const [id, point] of Object.entries(initial).filter(([id]) => id !== 'characterCount') as Array<[string, any]>) {
    if (![point.x, point.y, point.z].every(Number.isFinite)) throw new Error(`Package did not instantiate ${id}`);
  }

  const motion = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_set_obstacles_enabled(0);
    module._isoweb_set_obstacles_enabled(1);
    const get = (id: string, axis: 'x' | 'z') =>
      module.ccall(`isoweb_character_position_${axis}`, 'number', ['string'], [id]);
    const barrierBefore = get('demo-obstacle-barrier-left', 'x');
    for (let i = 0; i < 10; ++i) module._isoweb_tick(0.1);
    const barrierAfter = get('demo-obstacle-barrier-left', 'x');

    module._isoweb_set_obstacles_enabled(0);
    module._isoweb_set_obstacles_enabled(1);
    for (let i = 0; i < 12; ++i) module._isoweb_tick(0.1);
    const guillotineZ = get('demo-obstacle-guillotine', 'z');

    module._isoweb_set_obstacles_enabled(0);
    module._isoweb_set_obstacles_enabled(1);
    const leftCentre = get('demo-obstacle-barrier-left', 'x');
    const innerFace = leftCentre * 2 + 4.48;
    module.ccall(
      'isoweb_set_character_location',
      'number',
      ['string','string','string','string','number','number','number'],
      ['demo-character','demo','default','middle',innerFace + 0.24,1.9,0]
    );
    module._isoweb_tick(0);
    const playerX = get('demo-character', 'x');
    const playerY = module.ccall('isoweb_character_position_y', 'number', ['string'], ['demo-character']);
    return { barrierBefore, barrierAfter, guillotineZ, playerX, playerY };
  });

  if (Math.abs(motion.barrierAfter - motion.barrierBefore) < 0.02) {
    throw new Error(`Package-authored gate did not move: ${JSON.stringify(motion)}`);
  }
  if (motion.guillotineZ > 0.45) {
    throw new Error(`Package-authored guillotine did not descend: ${JSON.stringify(motion)}`);
  }
  if (Math.abs(motion.playerX) > 0.02 || Math.abs(motion.playerY - 2.4) > 0.02) {
    throw new Error(`Package-authored hazard did not respawn Character: ${JSON.stringify(motion)}`);
  }

  console.log('Package behaviour smoke passed: five dynamic bodies load from demo.isoworld, gate and guillotine animate, and authored hazards respawn the Character.');
} finally {
  await browser.close();
  server.stop(true);
}
