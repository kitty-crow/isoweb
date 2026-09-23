import { createRequire } from 'node:module';
import { extname, normalize } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json',
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

    const file = Bun.file(`site-threaded/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });

    return new Response(file, {
      headers: {
        'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream',
        'Cross-Origin-Opener-Policy': 'same-origin',
        'Cross-Origin-Embedder-Policy': 'require-corp'
      }
    });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
const errors: string[] = [];
const consoleMessages: string[] = [];
const failedRequests: string[] = [];

page.on('pageerror', (error: Error) => errors.push(error.stack || error.message));
page.on('console', (message: any) => consoleMessages.push(`${message.type()}: ${message.text()}`));
page.on('requestfailed', (request: any) => {
  failedRequests.push(`${request.url()}: ${request.failure()?.errorText ?? 'failed'}`);
});

try {
  console.log('[pthread-browser] booting cross-origin-isolated pthread build');
  await page.goto(`http://127.0.0.1:${server.port}/?presentation=canvas2d`, { waitUntil: 'domcontentloaded' });

  const preReady = await page.evaluate(() => ({
    isolated: crossOriginIsolated,
    sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
    hardwareConcurrency: navigator.hardwareConcurrency,
    moduleType: typeof (globalThis as any).Module,
    ready: document.documentElement.classList.contains('wasm-ready')
  }));
  console.log(`[pthread-browser] pre-ready ${JSON.stringify(preReady)}`);

  try {
    await page.waitForFunction(
      () => document.documentElement.classList.contains('wasm-ready'),
      undefined,
      { timeout: 60_000 }
    );
  } catch (error) {
    throw new Error(
      `Threaded WASM boot timed out. preReady=${JSON.stringify(preReady)} ` +
      `errors=${JSON.stringify(errors)} requests=${JSON.stringify(failedRequests)} ` +
      `console=${JSON.stringify(consoleMessages.slice(-30))}\n${String(error)}`
    );
  }

  const result = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    if (!crossOriginIsolated || typeof SharedArrayBuffer !== 'function') {
      throw new Error('Page is not cross-origin isolated with SharedArrayBuffer enabled.');
    }
    if (typeof module._isoweb_set_render_thread_limit !== 'function') {
      throw new Error('Render thread limit export is missing.');
    }

    module._isoweb_clear_entities();
    module._isoweb_reset_level();
    while (module._isoweb_active_level_index() > 0) module._isoweb_level_down();
    module._isoweb_reset_yaw();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();

    const created = module.ccall(
      'isoweb_create_character',
      'number',
      ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
      ['pthread-render-probe', 'demo', 'default', 'lower', 0.0, 0.0, 0.0]
    );
    if (created !== 1) throw new Error('Could not create pthread render probe Character.');

    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) throw new Error('Canvas is missing.');
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas 2D context is unavailable.');

    module._isoweb_set_render_thread_limit(1);
    module._isoweb_render();
    const reference = new Uint8ClampedArray(
      context.getImageData(0, 0, canvas.width, canvas.height).data
    );
    const referenceThreadCount = module._isoweb_last_render_thread_count();
    const referenceHelperRows = module._isoweb_last_render_helper_rows();

    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
    const parallel = context.getImageData(0, 0, canvas.width, canvas.height).data;
    const parallelThreadCount = module._isoweb_last_render_thread_count();
    const parallelHelperRows = module._isoweb_last_render_helper_rows();

    let mismatchCount = 0;
    let maxChannelDelta = 0;
    let firstMismatch = -1;
    for (let index = 0; index < reference.length; ++index) {
      const delta = Math.abs(reference[index] - parallel[index]);
      if (delta !== 0) {
        ++mismatchCount;
        if (firstMismatch < 0) firstMismatch = index;
        if (delta > maxChannelDelta) maxChannelDelta = delta;
      }
    }

    // Move the dynamic Character without changing the static camera/cache. This
    // render must take the retained-frame damage path. Then invalidate the
    // static cache via the existing thread-limit setter and render the exact
    // same state in full. Every output byte must match.
    const moved = module.ccall(
      'isoweb_set_character_location',
      'number',
      ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
      ['pthread-render-probe', 'demo', 'default', 'lower', 0.42, 0.18, 0.0]
    );
    if (moved !== 1) throw new Error('Could not move pthread render probe Character.');
    module._isoweb_render();
    const incremental = new Uint8ClampedArray(
      context.getImageData(0, 0, canvas.width, canvas.height).data
    );

    // setRenderThreadLimit intentionally invalidates the static cache even when
    // the numerical limit is unchanged, giving this test a same-state full redraw.
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
    const forcedFull = context.getImageData(0, 0, canvas.width, canvas.height).data;
    let damageMismatchCount = 0;
    let damageMaxChannelDelta = 0;
    let damageFirstMismatch = -1;
    for (let index = 0; index < incremental.length; ++index) {
      const delta = Math.abs(incremental[index] - forcedFull[index]);
      if (delta !== 0) {
        ++damageMismatchCount;
        if (damageFirstMismatch < 0) damageFirstMismatch = index;
        if (delta > damageMaxChannelDelta) damageMaxChannelDelta = delta;
      }
    }

    return {
      width: canvas.width,
      height: canvas.height,
      byteLength: reference.length,
      referenceThreadCount,
      referenceHelperRows,
      parallelThreadCount,
      parallelHelperRows,
      mismatchCount,
      maxChannelDelta,
      firstMismatch,
      damageMismatchCount,
      damageMaxChannelDelta,
      damageFirstMismatch,
      hardwareConcurrency: navigator.hardwareConcurrency
    };
  });

  if (result.referenceThreadCount !== 1 || result.referenceHelperRows !== 0) {
    throw new Error(`Forced single-thread reference used helpers: ${JSON.stringify(result)}`);
  }
  if (result.parallelThreadCount < 2 || result.parallelHelperRows <= 0) {
    throw new Error(`Parallel render did not execute helper rows: ${JSON.stringify(result)}`);
  }
  if (result.mismatchCount !== 0) {
    throw new Error(`Parallel render differs from same-binary single-thread reference: ${JSON.stringify(result)}`);
  }
  if (result.damageMismatchCount !== 0) {
    throw new Error(`Incremental damage render differs from forced full redraw: ${JSON.stringify(result)}`);
  }
  if (errors.length) throw new Error(errors.join('\n\n'));

  console.log(
    `Pthread browser smoke passed: 1-thread and ${result.parallelThreadCount}-thread renders are ` +
    `bit-identical across ${result.byteLength} RGBA bytes; ${result.parallelHelperRows} helper rows rendered.`
  );
} finally {
  await browser.close();
  server.stop(true);
}
