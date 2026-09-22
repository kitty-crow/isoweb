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
    const file = Bun.file('site/' + relative);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, {
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

async function inspect(
  query: string,
  configure?: (module: any) => void
): Promise<Record<string, any>> {
  const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
  try {
    await page.goto('http://127.0.0.1:' + server.port + '/' + query, { waitUntil: 'domcontentloaded' });
    await page.waitForFunction(
      () => document.documentElement.classList.contains('wasm-ready'),
      undefined,
      { timeout: 60_000 }
    );
    if (configure) await page.evaluate(configure, undefined);
    return await page.evaluate(() => {
      const module = (globalThis as any).Module;
      module._isoweb_set_render_thread_limit(4);
      module._isoweb_reset_level();
      module._isoweb_render();
      return {
        href: location.href,
        pathname: location.pathname,
        search: location.search,
        mode: (globalThis as any).isowebBrowserMode,
        runtime: (globalThis as any).isowebRuntimeMode,
        fallback: (globalThis as any).isowebRuntimeFallbackReason ?? null,
        isolated: crossOriginIsolated,
        sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
        controlled: Boolean(navigator.serviceWorker.controller),
        backend: module._isoweb_last_static_render_backend(),
        threads: module._isoweb_last_render_thread_count(),
        helpers: module._isoweb_last_render_helper_rows(),
        presentation: (window as any).isowebPresentationBackend ?? 'unknown'
      };
    });
  } finally {
    await page.close();
  }
}

const browser = await chromium.launch({ headless: true });

try {
  const ordinary = await inspect('');
  if (
    ordinary.mode.threaded || ordinary.mode.webgl ||
    ordinary.runtime !== 'single-thread' || ordinary.backend !== 0
  ) {
    throw new Error('Default root mode is wrong: ' + JSON.stringify(ordinary));
  }

  const threaded = await inspect('?threaded');
  if (
    !threaded.mode.threaded || threaded.mode.webgl ||
    threaded.runtime !== 'pthread' || !threaded.isolated ||
    !threaded.sharedArrayBuffer || threaded.backend !== 1 ||
    threaded.threads < 2 || threaded.helpers <= 0
  ) {
    throw new Error('?threaded mode is wrong: ' + JSON.stringify(threaded));
  }

  const webgl = await inspect('?webgl');
  if (
    webgl.mode.threaded || !webgl.mode.webgl ||
    webgl.runtime !== 'single-thread' || webgl.backend !== 2
  ) {
    throw new Error('?webgl mode is wrong: ' + JSON.stringify(webgl));
  }

  const hybrid = await inspect('?threaded&webgl');
  if (
    !hybrid.mode.threaded || !hybrid.mode.webgl ||
    hybrid.runtime !== 'pthread' || hybrid.backend !== 2
  ) {
    throw new Error('?threaded&webgl mode is wrong: ' + JSON.stringify(hybrid));
  }

  const hybridFallbackPage = await browser.newPage({ viewport: { width: 960, height: 720 } });
  await hybridFallbackPage.addInitScript(() => {
    const original = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function(type: any, ...args: any[]) {
      if (type === 'webgl2') return null;
      return original.call(this, type, ...args);
    } as any;
  });
  await hybridFallbackPage.goto(
    'http://127.0.0.1:' + server.port + '/?threaded&webgl',
    { waitUntil: 'domcontentloaded' }
  );
  await hybridFallbackPage.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 60_000 }
  );
  const hybridFallback = await hybridFallbackPage.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_reset_level();
    module._isoweb_render();
    return {
      runtime: (globalThis as any).isowebRuntimeMode,
      backend: module._isoweb_last_static_render_backend(),
      threads: module._isoweb_last_render_thread_count(),
      presentation: (window as any).isowebPresentationBackend
    };
  });
  await hybridFallbackPage.close();
  if (
    hybridFallback.runtime !== 'pthread' ||
    hybridFallback.backend !== 1 ||
    hybridFallback.threads < 2 ||
    hybridFallback.presentation !== 'canvas2d'
  ) {
    throw new Error('Hybrid WebGL failure did not fall back to MT CPU: ' + JSON.stringify(hybridFallback));
  }

  const threadFallbackPage = await browser.newPage({ viewport: { width: 960, height: 720 } });
  await threadFallbackPage.addInitScript(() => {
    try {
      Object.defineProperty(Navigator.prototype, 'serviceWorker', {
        configurable: true,
        get: () => undefined
      });
    } catch {}
  });
  await threadFallbackPage.goto(
    'http://127.0.0.1:' + server.port + '/?threaded',
    { waitUntil: 'domcontentloaded' }
  );
  await threadFallbackPage.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 60_000 }
  );
  const threadFallback = await threadFallbackPage.evaluate(() => ({
    requested: (globalThis as any).isowebBrowserMode,
    runtime: (globalThis as any).isowebRuntimeMode,
    reason: (globalThis as any).isowebRuntimeFallbackReason ?? ''
  }));
  await threadFallbackPage.close();
  if (
    !threadFallback.requested.threaded ||
    threadFallback.runtime !== 'single-thread' ||
    !String(threadFallback.reason).includes('Service workers')
  ) {
    throw new Error('Threading failure did not fall back to single-thread: ' + JSON.stringify(threadFallback));
  }

  const compositionPage = await browser.newPage({ viewport: { width: 960, height: 720 } });
  await compositionPage.goto(
    'http://127.0.0.1:' + server.port + '/?stats&webgl&dyaw&threaded&dzoom&unrelated=1',
    { waitUntil: 'domcontentloaded' }
  );
  await compositionPage.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 60_000 }
  );
  await compositionPage.locator('#rotate-clockwise').click();
  const composed = await compositionPage.evaluate(() => ({
    mode: (globalThis as any).isowebBrowserMode,
    runtime: (globalThis as any).isowebRuntimeMode,
    stats: Boolean(document.getElementById('performance-stats')),
    status: document.getElementById('view-status')?.textContent ?? ''
  }));
  await compositionPage.close();
  if (
    !composed.mode.detailedZoom || !composed.mode.detailedYaw || !composed.mode.stats ||
    !composed.mode.threaded || !composed.mode.webgl ||
    composed.runtime !== 'pthread' || !composed.stats ||
    !composed.status.includes('Camera 45 degrees') ||
    !composed.status.includes('detailed')
  ) {
    throw new Error('Composed query flags failed: ' + JSON.stringify(composed));
  }

  for (const [legacy, flag] of [['threaded', 'threaded'], ['webgl', 'webgl']] as const) {
    const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
    await page.goto(
      'http://127.0.0.1:' + server.port + '/' + legacy + '/?stats&dzoom',
      { waitUntil: 'domcontentloaded' }
    );
    await page.waitForFunction(
      expected => location.pathname === '/' && new URLSearchParams(location.search).has(expected),
      flag,
      { timeout: 60_000 }
    );
    const result = await page.evaluate(expected => ({
      pathname: location.pathname,
      params: Array.from(new URLSearchParams(location.search).keys()),
      expected
    }), flag);
    await page.close();
    if (
      result.pathname !== '/' ||
      !result.params.includes(flag) ||
      !result.params.includes('stats') ||
      !result.params.includes('dzoom')
    ) {
      throw new Error('Legacy route normalization failed: ' + JSON.stringify(result));
    }
  }

  console.log(
    'Query-mode browser matrix passed: ST CPU, pthread CPU, WebGL GPU, hybrid GPU+pthread, both fallbacks, flag composition, and legacy route normalization.'
  );
} finally {
  await browser.close();
  server.stop(true);
}
