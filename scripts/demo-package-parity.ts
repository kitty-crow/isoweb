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
  '.wasm': 'application/wasm',
  '.isoworld': 'application/octet-stream'
};

const server = Bun.serve({
  port: 0,
  idleTimeout: 120,
  async fetch(request) {
    const url = new URL(request.url);
    let pathname = decodeURIComponent(url.pathname);
    if (pathname === '/') pathname = '/index.html';
    if (pathname === '/favicon.ico') return new Response(null, { status: 204 });

    const relative = normalize(pathname).replace(/^[/\\]+/, '');
    if (relative.startsWith('..')) return new Response('Forbidden', { status: 403 });

    const file = Bun.file(\`site/\${relative}\`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });

    return new Response(file, {
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

const expectedFrames = [
  {"level":0,"yaw":0,"width":693,"height":520,"sha256":"2a97cd8c7f51ac0fd1df24a29c1f8dd42f6974c92cf45cc3a9282d0bccb46e34"},
  {"level":0,"yaw":1,"width":693,"height":520,"sha256":"ddda44454e67728deb9e0ff76ce75c89f3fb1d6705d6faa38dbfb477189f4e06"},
  {"level":0,"yaw":2,"width":693,"height":520,"sha256":"004b4d3f6a8241a3db636c3074bf75c68694ae04645314be4242b93b5744884e"},
  {"level":0,"yaw":3,"width":693,"height":520,"sha256":"e91fc4277e69e1557023cfdc45156acbd71976676b65d8c6d745dc72605f5a64"},
  {"level":1,"yaw":0,"width":693,"height":520,"sha256":"caab95cbe9abf28854639a8801762c04589af4bba32e0a193154d4f875008120"},
  {"level":1,"yaw":1,"width":693,"height":520,"sha256":"ff6eda3bfa3c53a77302377a09a77c8729854fba25744cc82fe254a97e5a2441"},
  {"level":1,"yaw":2,"width":693,"height":520,"sha256":"395b727cc873eed97aeacb8d3e59edffce08f0b007eae3a87a9ae95a6cf9b8c1"},
  {"level":1,"yaw":3,"width":693,"height":520,"sha256":"d371022975e6bac5ac85b5250654b168b97ed88917dfbdc3b48bd870466aff83"},
  {"level":2,"yaw":0,"width":693,"height":520,"sha256":"c920b88cde9f1b9b562d5042c20fcfa3a2b00ba19bbac719c37b279de720d463"},
  {"level":2,"yaw":1,"width":693,"height":520,"sha256":"90e0ea431544d4d1526f48a547350a3f08042866089b7c607a9e0d877ba78f09"},
  {"level":2,"yaw":2,"width":693,"height":520,"sha256":"549769c8b8f0587d66857390150842596e5d0d591e0e0421e23732f1d230b737"},
  {"level":2,"yaw":3,"width":693,"height":520,"sha256":"6671b49968d028a9e457875a12e69978c54c1f78d25b8dc0e46f164c7ffd5cc6"}
] as const;

async function capture(page: any, suffix: string) {
  await page.goto(\`http://127.0.0.1:\${server.port}/?webgl=0\${suffix}\`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('world-ready'),
    undefined,
    { timeout: 45_000 }
  );

  return page.evaluate(async () => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement;
    const context = canvas.getContext('2d');
    if (!context) throw new Error('Canvas2D context unavailable.');

    module._isoweb_set_obstacles_enabled(0);
    module._isoweb_clear_entities();
    module._isoweb_reset_zoom();
    module._isoweb_reset_camera();
    module._isoweb_reset_yaw();
    module._isoweb_reset_level();
    while (module._isoweb_active_level_index() > 0) module._isoweb_level_down();

    const result: Array<{ level: number; yaw: number; width: number; height: number; sha256: string }> = [];
    const levelCount = module._isoweb_level_count();

    for (let level = 0; level < levelCount; ++level) {
      if (module._isoweb_active_level_index() !== level) throw new Error(\`Could not select level \${level}\`);
      module._isoweb_reset_yaw();
      module._isoweb_reset_zoom();
      module._isoweb_reset_camera();

      for (let yaw = 0; yaw < 4; ++yaw) {
        module._isoweb_render();
        let guard = 0;
        while (module._isoweb_preview_needs_refinement() && guard++ < 2000) {
          module._isoweb_refine_preview(64);
        }
        module._isoweb_render();

        const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
        const digest = await crypto.subtle.digest(
          'SHA-256',
          pixels.buffer.slice(pixels.byteOffset, pixels.byteOffset + pixels.byteLength)
        );
        const sha256 = Array.from(new Uint8Array(digest))
          .map(value => value.toString(16).padStart(2, '0'))
          .join('');
        result.push({ level, yaw, width: canvas.width, height: canvas.height, sha256 });
        module._isoweb_rotate_clockwise();
      }

      if (level + 1 < levelCount) module._isoweb_level_up();
    }
    return result;
  });
}

function assertGolden(label: string, frames: Array<{ level: number; yaw: number; width: number; height: number; sha256: string }>) {
  if (frames.length !== expectedFrames.length) {
    throw new Error(\`\${label}: expected \${expectedFrames.length} frames, got \${frames.length}\`);
  }
  for (let index = 0; index < expectedFrames.length; ++index) {
    const expected = expectedFrames[index];
    const actual = frames[index];
    if (
      actual.level !== expected.level || actual.yaw !== expected.yaw ||
      actual.width !== expected.width || actual.height !== expected.height ||
      actual.sha256 !== expected.sha256
    ) {
      throw new Error(
        \`\${label}: golden mismatch at frame \${index}: expected \${JSON.stringify(expected)}, got \${JSON.stringify(actual)}\`
      );
    }
  }
}

const browser = await chromium.launch({ headless: true });
try {
  const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
  const compiledFrames = await capture(page, '');
  assertGolden('compiled demo.isoworld', compiledFrames);

  const sourceFrames = await capture(page, '&world=source');
  assertGolden('source demo-source.isoworld', sourceFrames);

  if (JSON.stringify(compiledFrames) !== JSON.stringify(sourceFrames)) {
    throw new Error('Compiled and source package renders diverged');
  }

  console.log(
    'Demo package render passed: compiled deployment and source compatibility paths both match 12 exact RGBA goldens.'
  );
} finally {
  await browser.close();
  server.stop(true);
}
