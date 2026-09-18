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

function serve(root: string, isolated: boolean) {
  return Bun.serve({
    port: 0,
    async fetch(request) {
      const url = new URL(request.url);
      let pathname = decodeURIComponent(url.pathname);
      if (pathname === '/') pathname = '/index.html';
      if (pathname === '/favicon.ico') return new Response(null, { status: 204 });

      const relative = normalize(pathname).replace(/^[/\\]+/, '');
      if (relative.startsWith('..')) return new Response('Forbidden', { status: 403 });

      const file = Bun.file(`${root}/${relative}`);
      if (!(await file.exists())) return new Response('Not found', { status: 404 });

      const headers: Record<string, string> = {
        'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream'
      };
      if (isolated) {
        headers['Cross-Origin-Opener-Policy'] = 'same-origin';
        headers['Cross-Origin-Embedder-Policy'] = 'require-corp';
      }
      return new Response(file, { headers });
    }
  });
}

type FrameState = {
  width: number;
  height: number;
  hashA: number;
  hashB: number;
  threadCount: number;
  helperRows: number;
};

async function deterministicFrame(page: any): Promise<FrameState> {
  return page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_clear_entities();
    module._isoweb_reset_level();
    while (module._isoweb_active_level_index() > 0) module._isoweb_level_down();
    module._isoweb_reset_yaw();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();
    module._isoweb_render();

    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) throw new Error('Canvas is missing.');
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas 2D context is unavailable.');

    const bytes = context.getImageData(0, 0, canvas.width, canvas.height).data;
    let hashA = 0x811c9dc5;
    let hashB = 0x9e3779b9;
    for (let index = 0; index < bytes.length; ++index) {
      const value = bytes[index];
      hashA = Math.imul(hashA ^ value, 0x01000193);
      hashB = Math.imul(hashB + value + index, 0x85ebca6b);
    }

    return {
      width: canvas.width,
      height: canvas.height,
      hashA: hashA >>> 0,
      hashB: hashB >>> 0,
      threadCount: module._isoweb_last_render_thread_count(),
      helperRows: module._isoweb_last_render_helper_rows()
    };
  });
}

const singleServer = serve('site', false);
const threadedServer = serve('site-threaded', true);
const browser = await chromium.launch({ headless: true });
const single = await browser.newPage({ viewport: { width: 960, height: 720 } });
const threaded = await browser.newPage({ viewport: { width: 960, height: 720 } });

const errors: string[] = [];
const threadedConsole: string[] = [];
const threadedRequests: string[] = [];
single.on('pageerror', (error: Error) => errors.push(`single: ${error.stack || error.message}`));
threaded.on('pageerror', (error: Error) => errors.push(`threaded: ${error.stack || error.message}`));
threaded.on('console', (message: any) => threadedConsole.push(`${message.type()}: ${message.text()}`));
threaded.on('requestfailed', (request: any) => {
  threadedRequests.push(`${request.url()}: ${request.failure()?.errorText ?? 'failed'}`);
});

try {
  console.log('[pthread-browser] booting single-thread reference');
  await single.goto(`http://127.0.0.1:${singleServer.port}/`, { waitUntil: 'domcontentloaded' });
  await single.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  console.log('[pthread-browser] booting cross-origin-isolated pthread build');
  await threaded.goto(`http://127.0.0.1:${threadedServer.port}/`, { waitUntil: 'domcontentloaded' });

  const preReady = await threaded.evaluate(() => ({
    isolated: crossOriginIsolated,
    sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
    hardwareConcurrency: navigator.hardwareConcurrency,
    moduleType: typeof (globalThis as any).Module,
    ready: document.documentElement.classList.contains('wasm-ready')
  }));
  console.log(`[pthread-browser] pre-ready ${JSON.stringify(preReady)}`);

  try {
    await threaded.waitForFunction(
      () => document.documentElement.classList.contains('wasm-ready'),
      undefined,
      { timeout: 60_000 }
    );
  } catch (error) {
    throw new Error(
      `Threaded WASM boot timed out. preReady=${JSON.stringify(preReady)} ` +
      `errors=${JSON.stringify(errors)} requests=${JSON.stringify(threadedRequests)} ` +
      `console=${JSON.stringify(threadedConsole.slice(-30))}\n${String(error)}`
    );
  }

  const isolation = await threaded.evaluate(() => ({
    isolated: crossOriginIsolated,
    sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
    hardwareConcurrency: navigator.hardwareConcurrency
  }));
  if (!isolation.isolated || !isolation.sharedArrayBuffer) {
    throw new Error(`Threaded page is not cross-origin isolated: ${JSON.stringify(isolation)}`);
  }

  const reference = await deterministicFrame(single);
  const parallel = await deterministicFrame(threaded);

  if (reference.threadCount !== 1 || reference.helperRows !== 0) {
    throw new Error(`Reference build unexpectedly used helpers: ${JSON.stringify(reference)}`);
  }
  if (parallel.threadCount < 2 || parallel.helperRows <= 0) {
    throw new Error(`Pthread build did not execute helper rows: ${JSON.stringify(parallel)}`);
  }
  if (
    reference.width !== parallel.width ||
    reference.height !== parallel.height ||
    reference.hashA !== parallel.hashA ||
    reference.hashB !== parallel.hashB
  ) {
    throw new Error(
      `Threaded frame differs from single-thread reference. single=${JSON.stringify(reference)} threaded=${JSON.stringify(parallel)}`
    );
  }
  if (errors.length) throw new Error(errors.join('\n\n'));

  console.log(
    `Pthread browser smoke passed: ${parallel.threadCount} render threads, ` +
    `${parallel.helperRows} helper rows, bit-identical ${parallel.width}x${parallel.height} frame.`
  );
} finally {
  await browser.close();
  singleServer.stop(true);
  threadedServer.stop(true);
}
