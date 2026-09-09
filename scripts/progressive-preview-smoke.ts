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
    return new Response(file, { headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' } });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 720, height: 720 } });

try {
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  // Keep the initial visibility checks and idle-gate checks in one JS turn so
  // the app's requestAnimationFrame loop cannot consume background-refinement
  // frames between assertions.
  const initialAndIdle = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_level_up(); // middle -> upper, exposing both lower previews
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    const initial = {
      potential: module._isoweb_preview_potential_texel_count(),
      demanded: module._isoweb_preview_demanded_texel_count(),
      coarse: module._isoweb_preview_coarse_sample_count(),
      refined: module._isoweb_preview_refined_sample_count(),
      needs: module._isoweb_preview_needs_refinement(),
      framePixels: canvas ? canvas.width * canvas.height : 0
    };

    const before = module._isoweb_preview_refined_sample_count();
    const results: number[] = [];
    for (let i = 0; i < 6; ++i) results.push(module._isoweb_refine_preview(1));
    const afterGate = module._isoweb_preview_refined_sample_count();
    const firstRefine = module._isoweb_refine_preview(1);
    const afterRefine = module._isoweb_preview_refined_sample_count();
    return {
      initial,
      idleGate: { before, results, afterGate, firstRefine, afterRefine }
    };
  });

  const { initial, idleGate } = initialAndIdle;
  if (!(initial.potential > 0)) throw new Error(`No lower-preview buffer on upper level: ${JSON.stringify(initial)}`);
  if (!(initial.framePixels > 0 && initial.potential >= initial.framePixels * 0.20)) {
    throw new Error(`Settled preview target is still too heavily downsampled: ${JSON.stringify(initial)}`);
  }
  if (!(initial.demanded > 0 && initial.demanded < initial.potential)) {
    throw new Error(`Preview demand was not visibility-culled: ${JSON.stringify(initial)}`);
  }
  if (!(initial.coarse > 0 && initial.coarse < initial.potential)) {
    throw new Error(`Immediate preview did not stay coarse/lazy: ${JSON.stringify(initial)}`);
  }
  if (initial.needs !== 1) throw new Error(`Visible preview had no deferred refinement work: ${JSON.stringify(initial)}`);

  // The first calls are intentionally suppressed by the idle delay. This is
  // what prevents panning/zooming from turning newly exposed preview tiles into
  // a synchronous frame-time spike.
  if (idleGate.results.some((value: number) => value !== 0) || idleGate.afterGate !== idleGate.before) {
    throw new Error(`Preview refined during the camera idle guard: ${JSON.stringify(idleGate)}`);
  }
  if (!(idleGate.firstRefine === 1 && idleGate.afterRefine > idleGate.afterGate)) {
    throw new Error(`Preview did not refine incrementally after becoming idle: ${JSON.stringify(idleGate)}`);
  }

  const panState = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const refinedBefore = module._isoweb_preview_refined_sample_count();
    module._isoweb_pan(28, 0);
    const refinedAfterPan = module._isoweb_preview_refined_sample_count();
    const coarseAfterPan = module._isoweb_preview_coarse_sample_count();
    const firstBackgroundAttempt = module._isoweb_refine_preview(1);
    const refinedAfterAttempt = module._isoweb_preview_refined_sample_count();
    return {
      refinedBefore,
      refinedAfterPan,
      coarseAfterPan,
      firstBackgroundAttempt,
      refinedAfterAttempt,
      demanded: module._isoweb_preview_demanded_texel_count(),
      potential: module._isoweb_preview_potential_texel_count()
    };
  });

  if (panState.refinedAfterPan !== panState.refinedBefore) {
    throw new Error(`Panning synchronously refined preview tiles: ${JSON.stringify(panState)}`);
  }
  if (!(panState.coarseAfterPan > 0)) {
    throw new Error(`Panning exposed no immediate coarse fallback: ${JSON.stringify(panState)}`);
  }
  if (panState.firstBackgroundAttempt !== 0 || panState.refinedAfterAttempt !== panState.refinedAfterPan) {
    throw new Error(`Panning did not re-arm the asynchronous idle guard: ${JSON.stringify(panState)}`);
  }
  if (!(panState.demanded > 0 && panState.demanded < panState.potential)) {
    throw new Error(`Panned preview stopped culling permanently hidden/off-view texels: ${JSON.stringify(panState)}`);
  }

  console.log('Progressive lower-preview smoke test passed.');
} finally {
  await browser.close();
  server.stop(true);
}
