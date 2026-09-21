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

function resolvePath(pathname: string): string | null {
  const clean = normalize(decodeURIComponent(pathname)).replace(/^[/\\]+/, '');
  if (clean.startsWith('..')) return null;

  for (const [prefix, root] of [
    ['single/', 'site/'],
    ['threaded/', 'site-threaded/'],
    ['adaptive/', 'site/adaptive/']
  ] as const) {
    if (clean === prefix.slice(0, -1)) return root + 'index.html';
    if (clean.startsWith(prefix)) {
      const suffix = clean.slice(prefix.length) || 'index.html';
      return root + suffix;
    }
  }
  return null;
}

const server = Bun.serve({
  port: 0,
  async fetch(request) {
    const url = new URL(request.url);
    if (url.pathname === '/favicon.ico') return new Response(null, { status: 204 });

    let pathname = url.pathname;
    if (pathname.endsWith('/')) pathname += 'index.html';
    const resolved = resolvePath(pathname);
    if (!resolved) return new Response('Forbidden', { status: 403 });

    const file = Bun.file(resolved);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, {
      headers: {
        'Content-Type': mimeTypes[extname(resolved)] ?? 'application/octet-stream',
        'Cross-Origin-Opener-Policy': 'same-origin',
        'Cross-Origin-Embedder-Policy': 'require-corp'
      }
    });
  }
});

const browser = await chromium.launch({ headless: true });

async function inspect(
  route: string,
  configure: () => void
): Promise<Record<string, unknown>> {
  const page = await browser.newPage({ viewport: { width: 960, height: 720 } });
  try {
    await page.goto(`http://127.0.0.1:${server.port}/${route}`, {
      waitUntil: 'domcontentloaded'
    });
    await page.waitForFunction(
      () => document.documentElement.classList.contains('wasm-ready'),
      undefined,
      { timeout: 60_000 }
    );

    await page.evaluate(configure);
    await page.waitForTimeout(400);

    return await page.evaluate(() => {
      const module = (globalThis as any).Module;
      const panel = document.getElementById('performance-stats');
      const rows = Object.fromEntries(
        Array.from(panel?.querySelectorAll('.stats-row') ?? []).map(row => {
          const key = row.querySelector('.stats-key')?.textContent ?? '';
          const value = row.querySelector('.stats-value')?.textContent ?? '';
          return [key, value];
        })
      );
      return {
        backend: module._isoweb_last_static_render_backend(),
        threads: module._isoweb_last_render_thread_count(),
        panel: Boolean(panel),
        rows,
        note: panel?.querySelector('.stats-note')?.textContent ?? ''
      };
    });
  } finally {
    await page.close();
  }
}


async function inspectNarrowHorizontalScroll(): Promise<Record<string, unknown>> {
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  try {
    await page.goto(`http://127.0.0.1:${server.port}/single/?stats&gpuStatic=0`, {
      waitUntil: 'domcontentloaded'
    });
    await page.waitForFunction(
      () => document.documentElement.classList.contains('wasm-ready'),
      undefined,
      { timeout: 60_000 }
    );
    await page.waitForTimeout(400);

    return await page.evaluate(() => {
      const panel = document.getElementById('performance-stats');
      const value = panel?.querySelector('.stats-value');
      if (!(panel instanceof HTMLElement)) {
        return { panel: false };
      }

      const style = getComputedStyle(panel);
      const valueStyle = value instanceof HTMLElement ? getComputedStyle(value) : null;
      const before = panel.scrollLeft;
      panel.scrollLeft = panel.scrollWidth;
      const after = panel.scrollLeft;
      const rect = panel.getBoundingClientRect();

      return {
        panel: true,
        viewportWidth: window.innerWidth,
        left: rect.left,
        right: rect.right,
        clientWidth: panel.clientWidth,
        scrollWidth: panel.scrollWidth,
        before,
        after,
        overflowX: style.overflowX,
        touchAction: style.touchAction,
        pointerEvents: style.pointerEvents,
        valueWhiteSpace: valueStyle?.whiteSpace ?? ''
      };
    });
  } finally {
    await page.close();
  }
}

try {
  const single = await inspect('single/?stats&gpuStatic=0', () => {
    const module = (globalThis as any).Module;
    module._isoweb_set_render_thread_limit(1);
    module._isoweb_render();
  });

  const threaded = await inspect('threaded/?stats&gpuStatic=0', () => {
    const module = (globalThis as any).Module;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_render();
  });

  const gpu = await inspect('adaptive/?stats', () => {
    const module = (globalThis as any).Module;
    module._isoweb_set_render_thread_limit(4);
    module._isoweb_reset_level();
    module._isoweb_render();
  });

  for (const [name, result, expected] of [
    ['single', single, 0],
    ['threaded', threaded, 1],
    ['gpu', gpu, 2]
  ] as const) {
    if (!result.panel) throw new Error(`${name}: stats panel missing`);
    if (result.backend !== expected) {
      throw new Error(`${name}: expected backend ${expected}, got ${JSON.stringify(result)}`);
    }

    const rows = result.rows as Record<string, string>;
    for (const required of ['FPS', 'Frame', 'Activity', 'Renderer', 'CPU', 'RAM', 'VRAM', 'GPU']) {
      if (!rows[required] || rows[required] === '…') {
        throw new Error(`${name}: missing ${required}: ${JSON.stringify(result)}`);
      }
    }
    if (!String(result.note).includes('per-core utilisation')) {
      throw new Error(`${name}: browser limitation disclosure missing`);
    }
  }

  if (!(single.rows as Record<string, string>).Renderer.includes('single-thread CPU')) {
    throw new Error(`single: wrong renderer label: ${JSON.stringify(single)}`);
  }
  if (!(threaded.rows as Record<string, string>).Renderer.includes('multi-thread CPU')) {
    throw new Error(`threaded: wrong renderer label: ${JSON.stringify(threaded)}`);
  }
  if (!(gpu.rows as Record<string, string>).Renderer.includes('WebGL2 GPU')) {
    throw new Error(`gpu: wrong renderer label: ${JSON.stringify(gpu)}`);
  }

  const narrow = await inspectNarrowHorizontalScroll();
  if (!narrow.panel) throw new Error('narrow: stats panel missing');
  if (
    Number(narrow.left) < -0.5 ||
    Number(narrow.right) > Number(narrow.viewportWidth) + 0.5
  ) {
    throw new Error(`narrow: panel escaped viewport: ${JSON.stringify(narrow)}`);
  }
  if (Number(narrow.scrollWidth) <= Number(narrow.clientWidth)) {
    throw new Error(`narrow: content did not overflow horizontally: ${JSON.stringify(narrow)}`);
  }
  if (Number(narrow.after) <= Number(narrow.before)) {
    throw new Error(`narrow: panel could not scroll horizontally: ${JSON.stringify(narrow)}`);
  }
  if (
    narrow.overflowX !== 'auto' ||
    narrow.touchAction !== 'pan-x' ||
    narrow.pointerEvents !== 'auto' ||
    narrow.valueWhiteSpace !== 'nowrap'
  ) {
    throw new Error(`narrow: scroll interaction styles are wrong: ${JSON.stringify(narrow)}`);
  }

  console.log(
    'Stats overlay passed in single-thread, pthread and WebGL2 GPU modes, including narrow horizontal scrolling.'
  );
} finally {
  await browser.close();
  server.stop(true);
}
