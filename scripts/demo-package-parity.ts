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
  '.wasm': 'application/wasm',
  '.isoworld': 'application/octet-stream'
};

const server = Bun.serve({
  port: 0,
  idleTimeout: 120,
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

try {
  const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
  await page.goto(`http://127.0.0.1:${server.port}/?webgl=0`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('world-ready'),
    undefined,
    { timeout: 45_000 }
  );

  const frames = await page.evaluate(async () => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement;
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas2D context unavailable.');

    module._isoweb_set_obstacles_enabled(0);
    module._isoweb_clear_entities();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();
    module._isoweb_reset_yaw();
    module._isoweb_reset_level();
    while (module._isoweb_active_level_index() > 0) module._isoweb_level_down();

    const result: Array<{ level: number; yaw: number; width: number; height: number; sha256: string }> = [];
    const levelCount = module._isoweb_level_count();

    for (let level = 0; level < levelCount; ++level) {
      if (module._isoweb_active_level_index() !== level) {
        throw new Error(`Could not select packaged level ${level}`);
      }

      module._isoweb_reset_yaw();
      module._isoweb_reset_zoom();
      module._isoweb_reset_camera();

      for (let yaw = 0; yaw < 4; ++yaw) {
        module._isoweb_render();
        let guard = 0;
        while (module._isoweb_preview_needs_refinement() && guard++ < 2000) {
          module._isoweb_refine_preview(64);
        }
        module._isoweb_render();

        const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
        const digest = await crypto.subtle.digest(
          'SHA-256',
          pixels.buffer.slice(pixels.byteOffset, pixels.byteOffset + pixels.byteLength)
        );
        const sha256 = Array.from(new Uint8Array(digest))
          .map(value => value.toString(16).padStart(2, '0'))
          .join('');
        result.push({ level, yaw, width: canvas.width, height: canvas.height, sha256 });

        module._isoweb_rotate_clockwise();
      }

      if (level + 1 < levelCount) module._isoweb_level_up();
    }

    return result;
  });

  if (frames.length !== 12) throw new Error(`Expected 12 packaged demo frames, got ${frames.length}.`);
  console.log('DEMO_PACKAGE_GOLDEN=' + JSON.stringify(frames));
} finally {
  await browser.close();
  server.stop(true);
}
