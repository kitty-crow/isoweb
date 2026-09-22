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

let releasePackage!: () => void;
const packageGate = new Promise<void>(resolve => { releasePackage = resolve; });
let packageRequested = false;

const server = Bun.serve({
  port: 0,
  async fetch(request) {
    const url = new URL(request.url);
    let pathname = decodeURIComponent(url.pathname);
    if (pathname === '/') pathname = '/index.html';
    if (pathname === '/favicon.ico') return new Response(null, { status: 204 });

    if (pathname.endsWith('/assets/demo.isoworld')) {
      packageRequested = true;
      await packageGate;
    }

    const relative = normalize(pathname).replace(/^[/\\]+/, '');
    if (relative.startsWith('..')) return new Response('Forbidden', { status: 403 });

    const file = Bun.file(`site/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });

    return new Response(file, {
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

async function captureBootstrap(page: any): Promise<number> {
  return page.evaluate(() => {
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

    const frames: Array<{
      level: number;
      yaw: number;
      width: number;
      height: number;
      pixels: Uint8ClampedArray;
    }> = [];

    const levelCount = module._isoweb_level_count();
    for (let level = 0; level < levelCount; ++level) {
      if (module._isoweb_active_level_index() !== level) {
        throw new Error(`Could not select bootstrap level ${level}`);
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

        frames.push({
          level,
          yaw,
          width: canvas.width,
          height: canvas.height,
          pixels: new Uint8ClampedArray(
            context.getImageData(0, 0, canvas.width, canvas.height).data
          )
        });

        module._isoweb_rotate_clockwise();
      }

      if (level + 1 < levelCount) module._isoweb_level_up();
    }

    (globalThis as any).__isowebBootstrapFrames = frames;
    return frames.length;
  });
}

async function comparePackaged(page: any): Promise<{ frames: number; bytes: number }> {
  return page.evaluate(() => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement;
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas2D context unavailable.');

    const baseline = (globalThis as any).__isowebBootstrapFrames as Array<{
      level: number;
      yaw: number;
      width: number;
      height: number;
      pixels: Uint8ClampedArray;
    }> | undefined;
    if (!baseline?.length) throw new Error('Bootstrap parity frames are missing.');

    module._isoweb_set_obstacles_enabled(0);
    module._isoweb_clear_entities();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();
    module._isoweb_reset_yaw();
    module._isoweb_reset_level();
    while (module._isoweb_active_level_index() > 0) module._isoweb_level_down();

    const levelCount = module._isoweb_level_count();
    let frameIndex = 0;
    let bytes = 0;

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

        const expected = baseline[frameIndex++];
        if (!expected) throw new Error('Packaged demo produced more frames than bootstrap demo.');
        if (
          expected.level !== level || expected.yaw !== yaw ||
          expected.width !== canvas.width || expected.height !== canvas.height
        ) {
          throw new Error(
            `Frame metadata changed at level ${level}, yaw ${yaw * 90} degrees.`
          );
        }

        const actual = context.getImageData(0, 0, canvas.width, canvas.height).data;
        bytes += actual.length;
        let mismatchedChannels = 0;
        let mismatchedPixels = 0;
        let maxDelta = 0;
        let firstMismatch = -1;

        for (let pixel = 0; pixel < canvas.width * canvas.height; ++pixel) {
          const base = pixel * 4;
          let pixelMismatch = false;
          for (let channel = 0; channel < 4; ++channel) {
            const index = base + channel;
            const delta = Math.abs(expected.pixels[index] - actual[index]);
            if (delta === 0) continue;
            ++mismatchedChannels;
            pixelMismatch = true;
            maxDelta = Math.max(maxDelta, delta);
            if (firstMismatch < 0) firstMismatch = index;
          }
          if (pixelMismatch) ++mismatchedPixels;
        }

        if (mismatchedChannels !== 0) {
          const pixel = Math.floor(firstMismatch / 4);
          throw new Error(
            `Packaged demo differs from bootstrap demo at level ${level}, yaw ${yaw * 90} degrees: ` +
            JSON.stringify({
              mismatchedChannels,
              mismatchedPixels,
              maxDelta,
              firstMismatch: {
                x: pixel % canvas.width,
                y: Math.floor(pixel / canvas.width),
                channel: firstMismatch % 4,
                bootstrap: expected.pixels[firstMismatch],
                package: actual[firstMismatch]
              }
            })
          );
        }

        module._isoweb_rotate_clockwise();
      }

      if (level + 1 < levelCount) module._isoweb_level_up();
    }

    if (frameIndex !== baseline.length) {
      throw new Error(
        `Packaged demo produced ${frameIndex} frames, bootstrap demo produced ${baseline.length}.`
      );
    }

    delete (globalThis as any).__isowebBootstrapFrames;
    return { frames: frameIndex, bytes };
  });
}

const browser = await chromium.launch({ headless: true });

try {
  const page = await browser.newPage({ viewport: { width: 960, height: 720 } });

  await page.goto(`http://127.0.0.1:${server.port}/?webgl=0`, { waitUntil: 'domcontentloaded' });

  await page.waitForFunction(
    () => {
      const module = (globalThis as any).Module;
      return typeof module?._isoweb_render === 'function' &&
        (window.isowebPresentedFrameCount ?? 0) > 0;
    },
    undefined,
    { timeout: 45_000 }
  );

  const requestDeadline = Date.now() + 10_000;
  while (!packageRequested && Date.now() < requestDeadline) await Bun.sleep(10);
  if (!packageRequested) throw new Error('Browser never requested demo.isoworld.');

  const bootstrapFrames = await captureBootstrap(page);
  if (bootstrapFrames !== 12) {
    throw new Error(`Expected 12 bootstrap frames, got ${bootstrapFrames}.`);
  }

  releasePackage();
  await page.waitForFunction(
    () => document.documentElement.classList.contains('world-ready'),
    undefined,
    { timeout: 30_000 }
  );

  const result = await comparePackaged(page);
  console.log(
    `Demo package parity passed: ${result.frames} frames and ${result.bytes} RGBA bytes match exactly ` +
    '(3 levels x 4 regular yaw angles).'
  );
} finally {
  releasePackage();
  await browser.close();
  server.stop(true);
}
