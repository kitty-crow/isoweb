import { createRequire } from 'node:module';
import { extname, normalize } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
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

    const contentType = mimeTypes[extname(relative)] ?? 'application/octet-stream';
    return new Response(file, { headers: { 'Content-Type': contentType } });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
const pageErrors: string[] = [];

page.on('pageerror', error => pageErrors.push(error.stack || error.message));

async function waitForWasmReady(): Promise<void> {
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );
}

try {
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await waitForWasmReady();

  const bootState = await page.evaluate(() => {
    const loading = document.getElementById('loading') as HTMLElement | null;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    const resetYaw = document.getElementById('reset-yaw') as HTMLButtonElement | null;
    const resetZoom = document.getElementById('reset-zoom') as HTMLButtonElement | null;
    const resetCamera = document.getElementById('reset-camera') as HTMLButtonElement | null;

    if (!loading || !canvas || !resetYaw || !resetZoom || !resetCamera) {
      return { ok: false, reason: 'Required boot elements are missing.' };
    }

    const context = canvas.getContext('2d');
    if (!context) return { ok: false, reason: 'Canvas 2D context is unavailable.' };

    const samplePoints = [
      [canvas.width * 0.2, canvas.height * 0.2],
      [canvas.width * 0.5, canvas.height * 0.5],
      [canvas.width * 0.8, canvas.height * 0.8]
    ];
    const samples = samplePoints.map(([x, y]) => {
      const pixel = context.getImageData(Math.floor(x), Math.floor(y), 1, 1).data;
      return `${pixel[0]},${pixel[1]},${pixel[2]},${pixel[3]}`;
    });

    return {
      ok: loading.hidden && canvas.width > 0 && canvas.height > 0 && samples.every(pixel => !pixel.endsWith(',0')),
      reason: loading.hidden ? '' : 'Loading indicator never hid.',
      resetYawDisabled: resetYaw.disabled,
      resetZoomDisabled: resetZoom.disabled,
      resetCameraDisabled: resetCamera.disabled,
      characterCount: globalThis.Module._isoweb_character_count()
    };
  });

  if (!bootState.ok) throw new Error(bootState.reason || 'WASM boot state is invalid.');
  if (!bootState.resetYawDisabled || !bootState.resetZoomDisabled || !bootState.resetCameraDisabled) {
    throw new Error('Default camera reset controls are not disabled after the first rendered frame.');
  }
  if (bootState.characterCount !== 1) {
    throw new Error(`Expected one character from demo-state.json, got ${bootState.characterCount}.`);
  }

  await page.locator('#rotate-clockwise').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 90 degrees'));

  const resetYawEnabled = await page.locator('#reset-yaw').isEnabled();
  if (!resetYawEnabled) throw new Error('Yaw reset did not become enabled after rotating the camera.');

  await page.goto(`http://127.0.0.1:${server.port}/?dyaw=1`, { waitUntil: 'domcontentloaded' });
  await waitForWasmReady();
  await page.locator('#rotate-clockwise').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 45 degrees'));

  await page.goto(`http://127.0.0.1:${server.port}/?dzoom=1`, { waitUntil: 'domcontentloaded' });
  await waitForWasmReady();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('zoom 1x detailed'));

  const reloadedCharacterCount = await page.evaluate(() => globalThis.Module._isoweb_character_count());
  if (reloadedCharacterCount !== 1) {
    throw new Error('Reload did not restore exactly one default-state character.');
  }

  if (pageErrors.length > 0) {
    throw new Error(`Browser page error(s):\n${pageErrors.join('\n\n')}`);
  }

  console.log('Browser WASM boot, default character state, normal yaw, detailed yaw, detailed zoom, and camera-state smoke test passed.');
} finally {
  await browser.close();
  server.stop(true);
}
