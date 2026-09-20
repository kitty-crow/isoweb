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
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 960, height: 720 } });

try {
  await page.goto(`http://127.0.0.1:${server.port}/adaptive/`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () =>
      document.documentElement.classList.contains('wasm-ready') &&
      (window as any).isowebAdaptiveContainer === 'pthread',
    undefined,
    { timeout: 60_000 }
  );

  const result = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_reset_level();
    module._isoweb_render();
    const middleBackend = module._isoweb_last_static_render_backend();

    module._isoweb_level_down();
    const lowerBackend = module._isoweb_last_static_render_backend();
    const lowerThreads = module._isoweb_last_render_thread_count();
    const lowerHelpers = module._isoweb_last_render_helper_rows();

    return {
      isolated: crossOriginIsolated,
      sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
      controlled: Boolean(navigator.serviceWorker.controller),
      container: (window as any).isowebAdaptiveContainer,
      presentation: window.isowebPresentationBackend ?? 'unknown',
      gpuStaticAvailable: window.isowebGpuStaticAvailable ?? false,
      middleBackend,
      lowerBackend,
      lowerThreads,
      lowerHelpers
    };
  });

  if (!result.isolated || !result.sharedArrayBuffer || !result.controlled) {
    throw new Error(`Adaptive route is not isolated: ${JSON.stringify(result)}`);
  }
  if (result.container !== 'pthread') {
    throw new Error(`Adaptive route did not choose pthread container: ${JSON.stringify(result)}`);
  }
  if (result.presentation !== 'webgl2' || !result.gpuStaticAvailable || result.middleBackend !== 2) {
    throw new Error(`Adaptive route did not choose GPU on supported scene: ${JSON.stringify(result)}`);
  }
  if (result.lowerBackend !== 1 || result.lowerThreads < 2 || result.lowerHelpers <= 0) {
    throw new Error(`Adaptive route did not fall back to MT CPU: ${JSON.stringify(result)}`);
  }

  console.log(`Adaptive Pages route passed: ${JSON.stringify(result)}`);
} finally {
  await browser.close();
  server.stop(true);
}
