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
    if (pathname.endsWith('/')) pathname += 'index.html';
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
const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
const errors: string[] = [];
page.on('pageerror', (error: Error) => errors.push(error.stack || error.message));

try {
  console.log('[threaded-pages] opening Pages-style route without server isolation headers');
  await page.goto(`http://127.0.0.1:${server.port}/threaded/`, { waitUntil: 'domcontentloaded' });

  await page.waitForFunction(
    () =>
      crossOriginIsolated &&
      typeof SharedArrayBuffer === 'function' &&
      document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 60_000 }
  );

  const result = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
    return {
      isolated: crossOriginIsolated,
      sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
      controlled: Boolean(navigator.serviceWorker.controller),
      hardwareConcurrency: navigator.hardwareConcurrency,
      threadCount: module._isoweb_last_render_thread_count(),
      helperRows: module._isoweb_last_render_helper_rows()
    };
  });

  if (!result.isolated || !result.sharedArrayBuffer || !result.controlled) {
    throw new Error(`Pages isolation worker did not activate: ${JSON.stringify(result)}`);
  }
  if (result.threadCount < 2 || result.helperRows <= 0) {
    throw new Error(`Threaded Pages route did not use helper threads: ${JSON.stringify(result)}`);
  }
  if (errors.length) throw new Error(errors.join('\n\n'));

  console.log(
    `Threaded Pages smoke passed: isolated=${result.isolated}, ` +
    `${result.threadCount} render threads, ${result.helperRows} helper rows.`
  );
} finally {
  await browser.close();
  server.stop(true);
}
