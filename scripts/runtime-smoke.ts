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

    const contentType = mimeTypes[extname(relative)] ?? 'application/octet-stream';
    return new Response(file, { headers: { 'Content-Type': contentType } });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
const pageErrors: string[] = [];
const consoleMessages: string[] = [];

page.on('pageerror', error => pageErrors.push(error.stack || error.message));
page.on('console', message => consoleMessages.push(`${message.type()}: ${message.text()}`));

async function waitForWasmReady(): Promise<void> {
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );
}

async function runtimeDiagnostics(): Promise<unknown> {
  return page.evaluate(() => {
    const module = (globalThis as any).Module;
    return {
      ready: document.documentElement.classList.contains('wasm-ready'),
      ccallType: typeof module?.ccall,
      characterCountType: typeof module?._isoweb_character_count,
      characterCount: typeof module?._isoweb_character_count === 'function'
        ? module._isoweb_character_count()
        : null,
      selectedCount: typeof module?._isoweb_selected_character_count === 'function'
        ? module._isoweb_selected_character_count()
        : null,
      needsTick: typeof module?._isoweb_needs_tick === 'function'
        ? module._isoweb_needs_tick()
        : null,
      loadingHidden: (document.getElementById('loading') as HTMLElement | null)?.hidden ?? null,
      canvas: (() => {
        const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
        return canvas ? { width: canvas.width, height: canvas.height } : null;
      })()
    };
  });
}

function browserMessages(): string {
  return [
    ...pageErrors.map(value => `pageerror: ${value}`),
    ...consoleMessages
  ].slice(-30).join('\n');
}

try {
  console.log('[runtime-smoke] booting default world');
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await waitForWasmReady();

  const bootState = await page.evaluate(() => {
    const loading = document.getElementById('loading') as HTMLElement | null;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    const resetYaw = document.getElementById('reset-yaw') as HTMLButtonElement | null;
    const resetZoom = document.getElementById('reset-zoom') as HTMLButtonElement | null;
    const resetCamera = document.getElementById('reset-camera') as HTMLButtonElement | null;

    if (!loading || !canvas || !resetYaw || !resetZoom || !resetCamera) {
      return { ok: false, reason: 'Required boot elements are missing.' };
    }

    const context = canvas.getContext('2d');
    if (!context) return { ok: false, reason: 'Canvas 2D context is unavailable.' };

    const samplePoints = [
      [canvas.width * 0.2, canvas.height * 0.2],
      [canvas.width * 0.5, canvas.height * 0.5],
      [canvas.width * 0.8, canvas.height * 0.8]
    ];
    const samples = samplePoints.map(([x, y]) => {
      const pixel = context.getImageData(Math.floor(x), Math.floor(y), 1, 1).data;
      return `${pixel[0]},${pixel[1]},${pixel[2]},${pixel[3]}`;
    });

    return {
      ok: loading.hidden && canvas.width > 0 && canvas.height > 0 && samples.every(pixel => !pixel.endsWith(',0')),
      reason: loading.hidden ? '' : 'Loading indicator never hid.',
      resetYawJoystick: !resetYaw.disabled && resetYaw.dataset.joystick === 'true',
      resetZoomJoystick: !resetZoom.disabled && resetZoom.dataset.joystick === 'true',
      resetCameraJoystick: !resetCamera.disabled && resetCamera.dataset.joystick === 'true'
    };
  });

  if (!bootState.ok) throw new Error(bootState.reason || 'WASM boot state is invalid.');
  if (!bootState.resetYawJoystick || !bootState.resetZoomJoystick || !bootState.resetCameraJoystick) {
    throw new Error('Centre camera discs are not interactive joysticks after the first rendered frame.');
  }

  console.log('[runtime-smoke] waiting for bundled JSON Character');
  try {
    await page.waitForFunction(() => {
      const module = (globalThis as any).Module;
      return typeof module?._isoweb_character_count === 'function' && module._isoweb_character_count() === 1;
    }, undefined, { timeout: 15_000 });
  } catch (error) {
    throw new Error(
      `Bundled Character load timed out. Diagnostics: ${JSON.stringify(await runtimeDiagnostics())}\n${browserMessages()}\n${String(error)}`
    );
  }

  console.log('[runtime-smoke] locating rendered Character through picker');
  const characterPoint = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) return null;
    module._isoweb_clear_selection();
    const step = Math.max(4, Math.floor(Math.min(canvas.width, canvas.height) / 70));
    for (let y = step; y < canvas.height - step; y += step) {
      for (let x = step; x < canvas.width - step; x += step) {
        if (module._isoweb_pointer_tap(x, y, 1) && module._isoweb_selected_character_count() === 1) {
          module._isoweb_clear_selection();
          return { x, y, width: canvas.width, height: canvas.height };
        }
      }
    }
    return null;
  });
  if (!characterPoint) {
    throw new Error(`Bundled no-art Character was not pickable. Diagnostics: ${JSON.stringify(await runtimeDiagnostics())}`);
  }

  const viewportBox = await page.locator('#viewport').boundingBox();
  if (!viewportBox) throw new Error('Viewport has no browser bounding box.');
  const characterCssX = viewportBox.x + characterPoint.x / characterPoint.width * viewportBox.width;
  const characterCssY = viewportBox.y + characterPoint.y / characterPoint.height * viewportBox.height;
  await page.mouse.click(characterCssX, characterCssY);
  try {
    await page.waitForFunction(
      () => (globalThis as any).Module._isoweb_selected_character_count() === 1,
      undefined,
      { timeout: 10_000 }
    );
  } catch (error) {
    throw new Error(
      `Browser mouse selection timed out. Diagnostics: ${JSON.stringify(await runtimeDiagnostics())}\n${browserMessages()}\n${String(error)}`
    );
  }

  const initialPosition = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    return {
      x: module.ccall('isoweb_character_position_x', 'number', ['string'], ['demo-character']),
      y: module.ccall('isoweb_character_position_y', 'number', ['string'], ['demo-character']),
      z: module.ccall('isoweb_character_position_z', 'number', ['string'], ['demo-character'])
    };
  });
  if (![initialPosition.x, initialPosition.y, initialPosition.z].every(Number.isFinite)) {
    throw new Error('Character runtime position getters did not resolve the JSON-created Character.');
  }

  console.log('[runtime-smoke] finding a real walkable destination through pointerTap');
  const firstDestination = await page.evaluate(({ characterX, characterY }) => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) return null;
    const step = Math.max(10, Math.floor(Math.min(canvas.width, canvas.height) / 28));
    for (let y = step; y < canvas.height - step; y += step) {
      for (let x = step; x < canvas.width - step; x += step) {
        if (Math.hypot(x - characterX, y - characterY) < 90) continue;
        const accepted = module._isoweb_pointer_tap(x, y, 0) !== 0;
        const selected = module._isoweb_selected_character_count();
        if (selected === 0) {
          module._isoweb_pointer_tap(characterX, characterY, 1);
          continue;
        }
        if (accepted && selected === 1 && module._isoweb_needs_tick() !== 0) {
          return { x, y, width: canvas.width, height: canvas.height };
        }
      }
    }
    return null;
  }, { characterX: characterPoint.x, characterY: characterPoint.y });

  if (!firstDestination) {
    throw new Error(`Selected Character found no commandable real walkable surface. Diagnostics: ${JSON.stringify(await runtimeDiagnostics())}`);
  }

  try {
    await page.waitForFunction(
      ({ x, y }) => {
        const module = (globalThis as any).Module;
        const nextX = module.ccall('isoweb_character_position_x', 'number', ['string'], ['demo-character']);
        const nextY = module.ccall('isoweb_character_position_y', 'number', ['string'], ['demo-character']);
        return Math.hypot(nextX - x, nextY - y) > 0.02;
      },
      initialPosition,
      { timeout: 10_000 }
    );
  } catch (error) {
    throw new Error(
      `Character position did not advance after a real-surface command. Diagnostics: ${JSON.stringify(await runtimeDiagnostics())}\n${browserMessages()}\n${String(error)}`
    );
  }

  const selectedAfterFirstMove = await page.evaluate(
    () => (globalThis as any).Module._isoweb_selected_character_count()
  );
  if (selectedAfterFirstMove !== 1) {
    throw new Error('Character selection was lost after issuing a movement command.');
  }

  console.log('[runtime-smoke] redirecting still-selected moving Character without reselecting');
  const redirectDestination = await page.evaluate(({ firstX, firstY, characterX, characterY }) => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) return null;
    const step = Math.max(10, Math.floor(Math.min(canvas.width, canvas.height) / 28));
    for (let y = canvas.height - step; y > step; y -= step) {
      for (let x = canvas.width - step; x > step; x -= step) {
        if (Math.hypot(x - firstX, y - firstY) < 120) continue;
        if (Math.hypot(x - characterX, y - characterY) < 90) continue;
        const accepted = module._isoweb_pointer_tap(x, y, 0) !== 0;
        const selected = module._isoweb_selected_character_count();
        if (selected === 0) {
          module._isoweb_pointer_tap(characterX, characterY, 1);
          continue;
        }
        if (accepted && selected === 1 && module._isoweb_needs_tick() !== 0) {
          return { x, y };
        }
      }
    }
    return null;
  }, {
    firstX: firstDestination.x,
    firstY: firstDestination.y,
    characterX: characterPoint.x,
    characterY: characterPoint.y
  });

  if (!redirectDestination) {
    throw new Error(`Still-selected moving Character found no accepted replacement destination. Diagnostics: ${JSON.stringify(await runtimeDiagnostics())}`);
  }

  const selectionAfterRedirect = await page.evaluate(
    () => (globalThis as any).Module._isoweb_selected_character_count()
  );
  if (selectionAfterRedirect !== 1) {
    throw new Error('Redirecting a selected Character unexpectedly deselected it.');
  }

  const stillMovingAfterRedirect = await page.evaluate(
    () => (globalThis as any).Module._isoweb_needs_tick() !== 0
  );
  if (!stillMovingAfterRedirect) {
    throw new Error('Redirected Character stopped instead of following the replacement route.');
  }

  console.log('[runtime-smoke] checking camera modes after Character interaction');
  await page.locator('#rotate-clockwise').click();
  await page.waitForFunction(
    () => document.getElementById('view-status')?.textContent?.includes('Camera 90 degrees'),
    undefined,
    { timeout: 10_000 }
  );

  const resetYawEnabled = await page.locator('#reset-yaw').isEnabled();
  if (!resetYawEnabled) throw new Error('Yaw centre joystick became disabled after rotating the camera.');

  await page.goto(`http://127.0.0.1:${server.port}/?dyaw=1`, { waitUntil: 'domcontentloaded' });
  await waitForWasmReady();
  await page.locator('#rotate-clockwise').click();
  await page.waitForFunction(
    () => document.getElementById('view-status')?.textContent?.includes('Camera 45 degrees'),
    undefined,
    { timeout: 10_000 }
  );

  await page.goto(`http://127.0.0.1:${server.port}/?dzoom=1`, { waitUntil: 'domcontentloaded' });
  await waitForWasmReady();
  await page.waitForFunction(
    () => document.getElementById('view-status')?.textContent?.includes('zoom 1x detailed'),
    undefined,
    { timeout: 10_000 }
  );

  if (pageErrors.length > 0) {
    throw new Error(`Browser page error(s):\n${pageErrors.join('\n\n')}`);
  }

  console.log('Browser WASM boot, JSON Character load, exact picking, real-surface movement, persistent selected redirects, centre joystick availability, yaw, detailed yaw, and detailed zoom smoke test passed.');
} finally {
  await browser.close();
  server.stop(true);
}
