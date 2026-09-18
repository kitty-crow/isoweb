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

type Comparison = {
  label: string;
  shiftedBuildCount: number;
  rebuiltBuildCount: number;
  mismatches: number;
  maxDelta: number;
};

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 960, height: 720 } });

try {
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  const results: Comparison[] = await page.evaluate(async () => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement;
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas context unavailable');

    const settlePreview = () => {
      let guard = 0;
      while (module._isoweb_preview_needs_refinement() && guard++ < 2000) {
        module._isoweb_refine_preview(64);
      }
    };

    const setCharacter = (x: number, y: number) => {
      const ok = module.ccall(
        'isoweb_set_character_location',
        'number',
        ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
        ['demo-character', 'demo', 'default', 'middle', x, y, 0]
      );
      if (!ok) throw new Error('Could not move demo-character');
    };

    const compareAt = (label: string, x: number, y: number) => {
      module._isoweb_reset_camera();
      module._isoweb_reset_zoom();
      module._isoweb_zoom_in();
      module._isoweb_zoom_in();
      setCharacter(x, y);
      module._isoweb_render();
      settlePreview();

      const beforePanBuildCount = module._isoweb_static_cache_build_count();
      // Camera::pan takes world units but commits them on the renderer pixel
      // grid. Use ordinary joystick-sized deltas across several frames so the
      // renderer exercises in-place static-cache shifting rather than hitting
      // the pan clamp and deliberately falling back to a full rebuild.
      for (let step = 0; step < 8; ++step) {
        module._isoweb_pan(0.16, 0.10);
      }
      settlePreview();
      const shiftedBuildCount = module._isoweb_static_cache_build_count();
      const shifted = new Uint8ClampedArray(
        context.getImageData(0, 0, canvas.width, canvas.height).data
      );

      if (shiftedBuildCount !== beforePanBuildCount) {
        throw new Error(
          `Pan performed a full rebuild instead of cache shift for ${label}: ` +
          `${beforePanBuildCount} -> ${shiftedBuildCount}`
        );
      }

      // This setter exists specifically as a deterministic test hook and
      // invalidates the static cache without altering camera/world state.
      module._isoweb_set_render_thread_limit(1);
      module._isoweb_render();
      settlePreview();
      const rebuiltBuildCount = module._isoweb_static_cache_build_count();
      const rebuilt = context.getImageData(0, 0, canvas.width, canvas.height).data;

      let mismatches = 0;
      let maxDelta = 0;
      for (let index = 0; index < shifted.length; ++index) {
        const delta = Math.abs(shifted[index] - rebuilt[index]);
        if (delta) {
          ++mismatches;
          if (delta > maxDelta) maxDelta = delta;
        }
      }
      return { label, shiftedBuildCount, rebuiltBuildCount, mismatches, maxDelta };
    };

    return [
      compareAt('cube-west', -2.05, 0.65),
      compareAt('cube-south', -1.05, -0.35),
      compareAt('sphere-west', 0.00, -0.25),
      compareAt('sphere-south', 1.05, -1.30),
      compareAt('sphere-east', 2.10, -0.25)
    ];
  });

  console.log(JSON.stringify(results, null, 2));
  const bad = results.filter(result => result.mismatches !== 0);
  if (bad.length) {
    throw new Error(
      'Panned static-cache frame differs from forced rebuild: ' +
      JSON.stringify(bad)
    );
  }

  console.log('Pan cache depth/compositing is bit-identical to a full rebuild around demo solids.');
} finally {
  await browser.close();
  server.stop(true);
}
