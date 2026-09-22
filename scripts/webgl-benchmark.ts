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
    const file = Bun.file(`site/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, {
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

async function measure(page: any, suffix: string) {
  await page.goto(`http://127.0.0.1:${server.port}/${suffix}`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  return page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_clear_entities();
    module._isoweb_reset_level();
    module._isoweb_reset_yaw();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();

    for (let warmup = 0; warmup < 5; ++warmup) module._isoweb_render();

    const total: number[] = [];
    const presentation: number[] = [];
    for (let sample = 0; sample < 30; ++sample) {
      const started = performance.now();
      module._isoweb_render();
      total.push(performance.now() - started);
      presentation.push(window.isowebLastPresentMilliseconds ?? NaN);
    }

    const average = (values: number[]) =>
      values.reduce((sum, value) => sum + value, 0) / values.length;

    return {
      backend: window.isowebPresentationBackend ?? 'unknown',
      totalMs: average(total),
      presentationMs: average(presentation),
      gpuMs: window.isowebLastGpuMilliseconds ?? null,
      estimatedRendererMs: average(total) - average(presentation),
      width: (document.getElementById('canvas') as HTMLCanvasElement).width,
      height: (document.getElementById('canvas') as HTMLCanvasElement).height
    };
  });
}

const browser = await chromium.launch({ headless: true });
try {
  const canvasPage = await browser.newPage({ viewport: { width: 960, height: 720 } });
  const webglPage = await browser.newPage({ viewport: { width: 960, height: 720 } });
  const canvas = await measure(canvasPage, '?presentation=canvas2d');
  const webgl = await measure(webglPage, '?gpuStatic=0');

  if (canvas.backend !== 'canvas2d' || webgl.backend !== 'webgl2') {
    throw new Error(`Unexpected backends: ${JSON.stringify({ canvas, webgl })}`);
  }
  if (![canvas.totalMs, canvas.presentationMs, webgl.totalMs, webgl.presentationMs].every(Number.isFinite)) {
    throw new Error(`Non-finite benchmark result: ${JSON.stringify({ canvas, webgl })}`);
  }

  console.log('WebGL presentation benchmark (diagnostic only; CI may use software rendering):');
  console.log(JSON.stringify({ canvas, webgl }, null, 2));
} finally {
  await browser.close();
  server.stop(true);
}
