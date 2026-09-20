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
      headers: {
        'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream'
      }
    });
  }
});

type Capture = {
  backend: string;
  width: number;
  height: number;
  gpuStaticAvailable: boolean;
  gpuStaticTraceCount: number;
  gpuStaticError: string | null;
  pixels: number[];
};

async function capture(page: any, suffix: string): Promise<Capture> {
  await page.goto(`http://127.0.0.1:${server.port}/${suffix}`, {
    waitUntil: 'domcontentloaded'
  });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  return page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_clear_entities();
    module._isoweb_reset_level();
    while (module._isoweb_active_level_index() > module._isoweb_default_level_index()) {
      module._isoweb_level_down();
    }
    while (module._isoweb_active_level_index() < module._isoweb_default_level_index()) {
      module._isoweb_level_up();
    }
    module._isoweb_reset_yaw();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();
    module._isoweb_render();

    let guard = 0;
    while (module._isoweb_preview_needs_refinement() && guard++ < 2000) {
      module._isoweb_refine_preview(64);
    }
    module._isoweb_render();

    const canvas = document.getElementById('canvas') as HTMLCanvasElement;
    const backend = window.isowebPresentationBackend ?? 'unknown';
    const width = canvas.width;
    const height = canvas.height;

    if (backend === 'canvas2d') {
      const context = canvas.getContext('2d');
      if (!context) throw new Error('Canvas2D context missing.');
      return {
        backend,
        width,
        height,
        gpuStaticAvailable: window.isowebGpuStaticAvailable ?? false,
        gpuStaticTraceCount: window.isowebGpuStaticTraceCount ?? 0,
        gpuStaticError: window.isowebGpuStaticError ?? null,
        pixels: Array.from(context.getImageData(0, 0, width, height).data)
      };
    }

    if (backend === 'webgl2') {
      const gl = canvas.getContext('webgl2');
      if (!gl) throw new Error('WebGL2 context missing.');
      const raw = new Uint8Array(width * height * 4);
      gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, raw);

      const topDown = new Uint8Array(raw.length);
      const rowBytes = width * 4;
      for (let y = 0; y < height; ++y) {
        const source = (height - 1 - y) * rowBytes;
        topDown.set(raw.subarray(source, source + rowBytes), y * rowBytes);
      }
      return {
        backend,
        width,
        height,
        gpuStaticAvailable: window.isowebGpuStaticAvailable ?? false,
        gpuStaticTraceCount: window.isowebGpuStaticTraceCount ?? 0,
        gpuStaticError: window.isowebGpuStaticError ?? null,
        pixels: Array.from(topDown)
      };
    }

    throw new Error(`Unknown presentation backend: ${backend}`);
  });
}

const browser = await chromium.launch({ headless: true });
try {
  const canvasPage = await browser.newPage({ viewport: { width: 960, height: 720 } });
  const webglPage = await browser.newPage({ viewport: { width: 960, height: 720 } });

  const canvas = await capture(canvasPage, '?webgl=0');
  const webgl = await capture(webglPage, '');

  if (canvas.backend !== 'canvas2d') {
    throw new Error(`Expected Canvas2D fallback, got ${canvas.backend}`);
  }
  if (webgl.backend !== 'webgl2') {
    throw new Error(`Expected WebGL2 backend, got ${webgl.backend}`);
  }
  if (!webgl.gpuStaticAvailable || webgl.gpuStaticTraceCount < 1) {
    throw new Error(`WebGL2 static tracer did not execute: ${JSON.stringify(webgl)}`);
  }
  if (canvas.gpuStaticTraceCount !== 0) {
    throw new Error(`Canvas2D fallback unexpectedly used GPU static tracing: ${JSON.stringify(canvas)}`);
  }
  if (canvas.width !== webgl.width || canvas.height !== webgl.height) {
    throw new Error(
      `Backend dimensions differ: canvas=${canvas.width}x${canvas.height}, ` +
      `webgl=${webgl.width}x${webgl.height}`
    );
  }

  let mismatches = 0;
  let maxDelta = 0;
  let firstMismatch = -1;
  for (let index = 0; index < canvas.pixels.length; ++index) {
    const delta = Math.abs(canvas.pixels[index] - webgl.pixels[index]);
    if (delta !== 0) {
      ++mismatches;
      if (firstMismatch < 0) firstMismatch = index;
      if (delta > maxDelta) maxDelta = delta;
    }
  }

  if (mismatches !== 0) {
    throw new Error(
      `WebGL2 frame differs from Canvas2D: mismatches=${mismatches}, ` +
      `maxDelta=${maxDelta}, firstMismatch=${firstMismatch}`
    );
  }

  console.log(
    `WebGL2 presenter matches Canvas2D exactly across ${canvas.pixels.length} RGBA bytes ` +
    `at ${canvas.width}x${canvas.height}.`
  );
} finally {
  await browser.close();
  server.stop(true);
}
