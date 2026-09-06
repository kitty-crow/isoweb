import { createRequire } from 'node:module';
import { extname, normalize } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.webp': 'image/webp',
  '.wasm': 'application/wasm'
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
    return new Response(file, {
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
const errors: string[] = [];
page.on('pageerror', error => errors.push(error.stack || error.message));

async function status(): Promise<string> {
  return (await page.locator('#view-status').textContent()) ?? '';
}

async function drag(id: string, dx: number, dy: number, holdMs = 320): Promise<void> {
  const box = await page.locator(id).boundingBox();
  if (!box) throw new Error(`${id} has no bounding box.`);
  const x = box.x + box.width * 0.5;
  const y = box.y + box.height * 0.5;
  await page.mouse.move(x, y);
  await page.mouse.down();
  await page.mouse.move(x + dx, y + dy, { steps: 4 });
  await page.waitForTimeout(holdMs);
  await page.mouse.up();
  await page.waitForTimeout(100);
}

try {
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  const layout = await page.evaluate(() => {
    const ids = [
      'reset-level', 'level-up', 'level-down',
      'reset-zoom', 'zoom-in', 'zoom-out',
      'reset-yaw', 'reset-camera'
    ];
    return ids.map(id => {
      const element = document.getElementById(id) as HTMLButtonElement | null;
      if (!element) return null;
      const rect = element.getBoundingClientRect();
      return {
        id,
        left: rect.left,
        top: rect.top,
        width: rect.width,
        height: rect.height,
        centreX: rect.left + rect.width * 0.5,
        centreY: rect.top + rect.height * 0.5,
        disabled: element.disabled,
        joystick: element.dataset.joystick
      };
    });
  });

  if (layout.some(value => !value)) throw new Error('A controller hitbox is missing.');
  for (const id of ['reset-level', 'reset-zoom', 'reset-yaw', 'reset-camera']) {
    const value = layout.find(item => item?.id === id)!;
    if (value.disabled) throw new Error(`${id} is disabled and cannot act as a joystick.`);
    if (value.joystick !== 'true') throw new Error(`${id} is not marked as a joystick.`);
  }

  const level = layout.find(value => value?.id === 'reset-level')!;
  const levelUp = layout.find(value => value?.id === 'level-up')!;
  const levelDown = layout.find(value => value?.id === 'level-down')!;
  const zoom = layout.find(value => value?.id === 'reset-zoom')!;
  const zoomIn = layout.find(value => value?.id === 'zoom-in')!;
  const zoomOut = layout.find(value => value?.id === 'zoom-out')!;

  if (!(zoom.left < level.left)) throw new Error('Zoom must remain on the left and Z-level on the right.');
  if (!(zoomOut.centreX < zoom.centreX && zoom.centreX < zoomIn.centreX)) {
    throw new Error('Zoom controls are not arranged horizontally as minus, centre, plus.');
  }
  if (Math.abs(zoomOut.centreY - zoom.centreY) > 3 || Math.abs(zoomIn.centreY - zoom.centreY) > 3) {
    throw new Error('Zoom minus/centre/plus are not on one horizontal row.');
  }
  if (!(levelUp.centreY < level.centreY && level.centreY < levelDown.centreY)) {
    throw new Error('Z-level up/centre/down are not arranged vertically.');
  }

  console.log('[joystick-browser] tap reset remains functional');
  await page.locator('#rotate-clockwise').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 90 degrees'));
  await page.locator('#reset-yaw').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 0 degrees'));

  console.log('[joystick-browser] pan centre disc continuously without rebuilding static world');
  const buildsBeforePan = await page.evaluate(() => (globalThis as any).Module._isoweb_static_cache_build_count());
  await drag('#reset-camera', 32, 0, 360);
  const pannedStatus = await status();
  const panMatch = pannedStatus.match(/pan X (-?\d+(?:\.\d+)?); Y (-?\d+(?:\.\d+)?)/);
  if (!panMatch || (Math.abs(Number(panMatch[1])) < 0.01 && Math.abs(Number(panMatch[2])) < 0.01)) {
    throw new Error(`Pan joystick did not move the camera: ${pannedStatus}`);
  }
  const buildsAfterPan = await page.evaluate(() => (globalThis as any).Module._isoweb_static_cache_build_count());
  if (buildsAfterPan !== buildsBeforePan) {
    throw new Error(`Continuous pan rebuilt the full static world: ${buildsBeforePan} -> ${buildsAfterPan}`);
  }
  await page.locator('#reset-camera').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('pan X 0.00; Y 0.00'));

  console.log('[joystick-browser] yaw centre disc');
  await drag('#reset-yaw', 32, 0);
  if ((await status()).includes('Camera 0 degrees')) throw new Error('Yaw joystick did not rotate the camera.');
  await page.locator('#reset-yaw').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 0 degrees'));

  console.log('[joystick-browser] zoom centre disc');
  await drag('#reset-zoom', 32, 0);
  if ((await status()).includes('zoom 1x')) throw new Error('Zoom joystick did not change zoom.');
  await page.locator('#reset-zoom').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('zoom 1x'));

  console.log('[joystick-browser] Z-level centre disc');
  const initialLevel = await page.evaluate(() => (globalThis as any).Module._isoweb_active_level_index());
  await drag('#reset-level', 0, -32);
  const raisedLevel = await page.evaluate(() => (globalThis as any).Module._isoweb_active_level_index());
  if (!(raisedLevel > initialLevel)) {
    throw new Error(`Level joystick did not move upward: ${initialLevel} -> ${raisedLevel}`);
  }
  await page.locator('#reset-level').click();
  await page.waitForFunction(
    () => (globalThis as any).Module._isoweb_active_level_index() ===
      (globalThis as any).Module._isoweb_default_level_index()
  );

  if (errors.length) throw new Error(errors.join('\n\n'));
  console.log('Centre joystick browser smoke passed: original top layout, tap resets, pan/yaw/zoom/Z drags, and pan cache reuse.');
} finally {
  await browser.close();
  server.stop(true);
}
