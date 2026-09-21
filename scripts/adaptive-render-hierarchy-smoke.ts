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

try {
  await page.goto(`http://127.0.0.1:${server.port}/?gpuStatic=1`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () =>
      crossOriginIsolated &&
      document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 60_000 }
  );

  const result = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const gpuTrace = (globalThis as any).isowebTraceStaticWebGl;
    if (window.isowebPresentationBackend !== 'webgl2') {
      throw new Error(`Expected WebGL2 presentation, got ${window.isowebPresentationBackend}`);
    }
    if (!window.isowebGpuStaticAvailable || typeof gpuTrace !== 'function') {
      throw new Error('WebGL2 static tracer is unavailable.');
    }

    const backend = () => module._isoweb_last_static_render_backend();
    const threads = () => module._isoweb_last_render_thread_count();
    const helpers = () => module._isoweb_last_render_helper_rows();

    while (module._isoweb_active_level_index() > module._isoweb_default_level_index()) {
      module._isoweb_level_down();
    }
    while (module._isoweb_active_level_index() < module._isoweb_default_level_index()) {
      module._isoweb_level_up();
    }

    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
    const gpu = { backend: backend(), threads: threads(), helpers: helpers() };

    (globalThis as any).isowebTraceStaticWebGl = undefined;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
    const mt = { backend: backend(), threads: threads(), helpers: helpers() };

    module._isoweb_set_render_thread_limit(1);
    module._isoweb_render();
    const st = { backend: backend(), threads: threads(), helpers: helpers() };

    (globalThis as any).isowebTraceStaticWebGl = gpuTrace;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
    const gpuRestored = { backend: backend(), threads: threads(), helpers: helpers() };

    module._isoweb_level_down();
    const unsupportedScene = { backend: backend(), threads: threads(), helpers: helpers() };

    return {
      isolated: crossOriginIsolated,
      hardwareConcurrency: navigator.hardwareConcurrency,
      gpu,
      mt,
      st,
      gpuRestored,
      unsupportedScene
    };
  });

  if (result.gpu.backend !== 2) {
    throw new Error(`GPU stage did not select WebGL2: ${JSON.stringify(result)}`);
  }
  if (result.mt.backend !== 1 || result.mt.threads < 2 || result.mt.helpers <= 0) {
    throw new Error(`GPU failure did not fall back to multithread CPU: ${JSON.stringify(result)}`);
  }
  if (result.st.backend !== 0 || result.st.threads !== 1 || result.st.helpers !== 0) {
    throw new Error(`Forced single-thread fallback is wrong: ${JSON.stringify(result)}`);
  }
  if (result.gpuRestored.backend !== 2) {
    throw new Error(`Restored GPU did not regain priority: ${JSON.stringify(result)}`);
  }
  if (
    result.unsupportedScene.backend !== 1 ||
    result.unsupportedScene.threads < 2 ||
    result.unsupportedScene.helpers <= 0
  ) {
    throw new Error(`Unsupported GPU scene did not fall back to MT CPU: ${JSON.stringify(result)}`);
  }

  console.log(
    'Adaptive hierarchy passed: GPU -> MT CPU -> ST CPU, with per-scene MT fallback. ' +
    JSON.stringify(result)
  );
} finally {
  await browser.close();
  server.stop(true);
}
