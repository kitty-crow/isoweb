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
      staticCacheBuilds: typeof module?._isoweb_static_cache_build_count === 'function'
        ? module._isoweb_static_cache_build_count()
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
  await page.goto(`http://127.0.0.1:${server.port}/?webgl=0`, { waitUntil: 'domcontentloaded' });
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

  console.log('[runtime-smoke] verifying exact static scene cache reuse');
  const cacheState = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const initial = module._isoweb_static_cache_build_count();
    module._isoweb_render();
    const unchangedRedraw = module._isoweb_static_cache_build_count();
    module._isoweb_rotate_clockwise();
    const afterRotate = module._isoweb_static_cache_build_count();
    module._isoweb_render();
    const unchangedRotatedRedraw = module._isoweb_static_cache_build_count();
    module._isoweb_reset_yaw();
    const afterReset = module._isoweb_static_cache_build_count();
    return { initial, unchangedRedraw, afterRotate, unchangedRotatedRedraw, afterReset };
  });

  if (cacheState.initial < 1) {
    throw new Error(`Static scene cache was never built: ${JSON.stringify(cacheState)}`);
  }
  if (cacheState.unchangedRedraw !== cacheState.initial) {
    throw new Error(`Unchanged redraw rebuilt the static scene: ${JSON.stringify(cacheState)}`);
  }
  if (cacheState.afterRotate !== cacheState.initial + 1) {
    throw new Error(`Camera rotation did not invalidate static scene exactly once: ${JSON.stringify(cacheState)}`);
  }
  if (cacheState.unchangedRotatedRedraw !== cacheState.afterRotate) {
    throw new Error(`Repeated rotated redraw rebuilt static scene: ${JSON.stringify(cacheState)}`);
  }
  if (cacheState.afterReset !== cacheState.afterRotate + 1) {
    throw new Error(`Yaw reset did not invalidate static scene exactly once: ${JSON.stringify(cacheState)}`);
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

  console.log('[runtime-smoke] proving vertical pan cache matches a full rebuild');
  const panParity = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) return { ok: false, reason: 'canvas missing' };
    const context = canvas.getContext('2d');
    if (!context) return { ok: false, reason: '2d context missing' };

    module._isoweb_reset_yaw();
    module._isoweb_reset_zoom();
    module._isoweb_zoom_in();
    module._isoweb_render();

    const buildsBeforePan = module._isoweb_static_cache_build_count();
    module._isoweb_pan(0, 0.75);
    const buildsAfterPan = module._isoweb_static_cache_build_count();
    const shifted = context.getImageData(0, 0, canvas.width, canvas.height).data.slice();

    // setRenderThreadLimit invalidates the static cache without changing the
    // camera. Re-rendering therefore gives an exact same-view full rebuild to
    // compare against the pan-shifted cache.
    module._isoweb_set_render_thread_limit(1);
    module._isoweb_render();
    const buildsAfterRebuild = module._isoweb_static_cache_build_count();
    const rebuilt = context.getImageData(0, 0, canvas.width, canvas.height).data;

    let differingChannels = 0;
    let differingPixels = 0;
    let maxDelta = 0;
    for (let index = 0; index < rebuilt.length; index += 4) {
      let pixelDifferent = false;
      for (let channel = 0; channel < 3; ++channel) {
        const delta = Math.abs(Number(shifted[index + channel]) - Number(rebuilt[index + channel]));
        if (delta !== 0) {
          ++differingChannels;
          pixelDifferent = true;
          maxDelta = Math.max(maxDelta, delta);
        }
      }
      if (pixelDifferent) ++differingPixels;
    }

    return {
      ok: true,
      buildsBeforePan,
      buildsAfterPan,
      buildsAfterRebuild,
      differingChannels,
      differingPixels,
      maxDelta,
      width: canvas.width,
      height: canvas.height
    };
  });
  console.log('[flat-floor-diagnostic] pan parity', JSON.stringify(panParity));
  if (!panParity.ok) throw new Error(`Pan parity setup failed: ${JSON.stringify(panParity)}`);
  if (panParity.buildsAfterPan !== panParity.buildsBeforePan) {
    throw new Error(`Vertical pan did not exercise the static-cache shift path: ${JSON.stringify(panParity)}`);
  }
  if (panParity.buildsAfterRebuild !== panParity.buildsAfterPan + 1) {
    throw new Error(`Forced same-view rebuild did not rebuild exactly once: ${JSON.stringify(panParity)}`);
  }
  const staticOnlyPage = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const staticOnlyParity = await (async () => {
    try {
      await staticOnlyPage.goto(`http://127.0.0.1:${server.port}/?webgl=0`, {
        waitUntil: 'domcontentloaded'
      });
      await staticOnlyPage.waitForFunction(
        () => document.documentElement.classList.contains('wasm-ready'),
        undefined,
        { timeout: 60_000 }
      );
      await staticOnlyPage.waitForFunction(
        () => (globalThis as any).Module?._isoweb_character_count?.() === 1,
        undefined,
        { timeout: 15_000 }
      );

      return await staticOnlyPage.evaluate(() => {
        const module = (globalThis as any).Module;
        const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
        if (!canvas) return { ok: false, reason: 'canvas missing' };
        const context = canvas.getContext('2d');
        if (!context) return { ok: false, reason: '2d context missing' };

        module._isoweb_clear_entities();
        module._isoweb_reset_yaw();
        module._isoweb_reset_zoom();
        module._isoweb_zoom_in();
        module._isoweb_render();

        const buildsBeforePan = module._isoweb_static_cache_build_count();
        module._isoweb_pan(0, 0.75);
        const buildsAfterPan = module._isoweb_static_cache_build_count();
        const shifted = context.getImageData(0, 0, canvas.width, canvas.height).data.slice();

        module._isoweb_set_render_thread_limit(1);
        module._isoweb_render();
        const buildsAfterRebuild = module._isoweb_static_cache_build_count();
        const rebuilt = context.getImageData(0, 0, canvas.width, canvas.height).data;

        let differingPixels = 0;
        let maxDelta = 0;
        for (let index = 0; index < rebuilt.length; index += 4) {
          let pixelDifferent = false;
          for (let channel = 0; channel < 3; ++channel) {
            const delta = Math.abs(Number(shifted[index + channel]) - Number(rebuilt[index + channel]));
            if (delta !== 0) {
              pixelDifferent = true;
              maxDelta = Math.max(maxDelta, delta);
            }
          }
          if (pixelDifferent) ++differingPixels;
        }

        return {
          ok: true,
          buildsBeforePan,
          buildsAfterPan,
          buildsAfterRebuild,
          differingPixels,
          maxDelta
        };
      });
    } finally {
      await staticOnlyPage.close();
    }
  })();
  console.log('[flat-floor-diagnostic] static-only pan parity', JSON.stringify(staticOnlyParity));

  if (!staticOnlyParity.ok) {
    throw new Error(`Static-only pan parity setup failed: ${JSON.stringify(staticOnlyParity)}`);
  }
  if (panParity.differingPixels !== 0) {
    console.log(
      '[flat-floor-diagnostic] pan-cache parity mismatch is static-only too; not using it as the Character regression'
    );
  }

  // Return to the exact default view before continuing the interaction smoke.
  await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module._isoweb_reset_camera();
    module._isoweb_reset_zoom();
    module._isoweb_reset_yaw();
    module._isoweb_render();
  });

  console.log('[flat-floor-diagnostic] comparing Character occlusion after cache shift vs full rebuild');
  const occlusionParityPage = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const occlusionParity = await (async () => {
    try {
      await occlusionParityPage.goto(`http://127.0.0.1:${server.port}/?webgl=0`, {
        waitUntil: 'domcontentloaded'
      });
      await occlusionParityPage.waitForFunction(
        () => document.documentElement.classList.contains('wasm-ready'),
        undefined,
        { timeout: 60_000 }
      );
      await occlusionParityPage.waitForFunction(
        () => (globalThis as any).Module?._isoweb_character_count?.() === 1,
        undefined,
        { timeout: 15_000 }
      );

      return await occlusionParityPage.evaluate(() => {
        const module = (globalThis as any).Module;
        const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
        if (!canvas) return { ok: false, reason: 'canvas missing' };
        const context = canvas.getContext('2d');
        if (!context) return { ok: false, reason: '2d context missing' };

        const frame = () =>
          context.getImageData(0, 0, canvas.width, canvas.height).data.slice();

        const createDiagnosticCharacter = () => {
          module._isoweb_clear_entities();
          const created = module.ccall(
            'isoweb_create_character',
            'number',
            ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
            ['diagnostic-character', 'demo', 'default', 'middle', 0.0, 2.4, 0.0]
          );
          if (!created) throw new Error('unable to create diagnostic Character');
          module.ccall(
            'isoweb_set_character_hitbox',
            'number',
            ['string', 'number', 'number', 'number', 'number', 'number', 'number'],
            ['diagnostic-character', -0.28, -0.20, 0.0, 0.28, 0.20, 1.65]
          );
          module.ccall(
            'isoweb_set_character_forward',
            'number',
            ['string', 'number', 'number'],
            ['diagnostic-character', 0.0, 1.0]
          );
        };

        const maskBetween = (withCharacter: Uint8ClampedArray, background: Uint8ClampedArray) => {
          const mask = new Uint8Array(canvas.width * canvas.height);
          let pixels = 0;
          for (let pixel = 0; pixel < mask.length; ++pixel) {
            const base = pixel * 4;
            const delta =
              Math.abs(withCharacter[base] - background[base]) +
              Math.abs(withCharacter[base + 1] - background[base + 1]) +
              Math.abs(withCharacter[base + 2] - background[base + 2]);
            if (delta >= 8) {
              mask[pixel] = 1;
              ++pixels;
            }
          }
          return { mask, pixels };
        };

        module._isoweb_reset_camera();
        module._isoweb_reset_yaw();
        module._isoweb_reset_zoom();
        module._isoweb_zoom_in();

        createDiagnosticCharacter();
        module._isoweb_render();
        module._isoweb_pan(0, 0.75);

        const shiftedWithCharacter = frame();
        module._isoweb_clear_entities();
        module._isoweb_render();
        const shiftedBackground = frame();
        const shiftedMask = maskBetween(shiftedWithCharacter, shiftedBackground);

        createDiagnosticCharacter();
        module._isoweb_set_render_thread_limit(1);
        module._isoweb_render();
        const rebuiltWithCharacter = frame();
        module._isoweb_clear_entities();
        module._isoweb_render();
        const rebuiltBackground = frame();
        const rebuiltMask = maskBetween(rebuiltWithCharacter, rebuiltBackground);

        let xorPixels = 0;
        let shiftedOnly = 0;
        let rebuiltOnly = 0;
        for (let i = 0; i < shiftedMask.mask.length; ++i) {
          if (shiftedMask.mask[i] === rebuiltMask.mask[i]) continue;
          ++xorPixels;
          if (shiftedMask.mask[i]) ++shiftedOnly;
          else ++rebuiltOnly;
        }

        return {
          ok: true,
          shiftedPixels: shiftedMask.pixels,
          rebuiltPixels: rebuiltMask.pixels,
          xorPixels,
          shiftedOnly,
          rebuiltOnly,
          cacheBuilds: module._isoweb_static_cache_build_count()
        };
      });
    } finally {
      await occlusionParityPage.close();
    }
  })();
  console.log('[flat-floor-diagnostic] occlusion-parity', JSON.stringify(occlusionParity));

  console.log('[flat-floor-diagnostic] scanning flat-floor Character silhouette');
  const silhouettePage = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const silhouetteScan = await (async () => {
    try {
      await silhouettePage.goto(`http://127.0.0.1:${server.port}/`, {
        waitUntil: 'domcontentloaded'
      });
      await silhouettePage.waitForFunction(
        () => document.documentElement.classList.contains('wasm-ready'),
        undefined,
        { timeout: 60_000 }
      );
      await silhouettePage.waitForFunction(
        () => (globalThis as any).Module?._isoweb_character_count?.() === 1,
        undefined,
        { timeout: 15_000 }
      );

      return await silhouettePage.evaluate(() => {
        const module = (globalThis as any).Module;
        const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
        if (!canvas) return { ok: false, reason: 'canvas missing' };
        const gl = canvas.getContext('webgl2');
        if (!gl) return { ok: false, reason: 'WebGL2 context missing' };
        const readFrame = () => {
          const pixels = new Uint8Array(canvas.width * canvas.height * 4);
          gl.readPixels(
            0, 0, canvas.width, canvas.height,
            gl.RGBA, gl.UNSIGNED_BYTE, pixels
          );
          return pixels;
        };

        module._isoweb_reset_camera();
        module._isoweb_reset_yaw();
        module._isoweb_reset_zoom();
        module._isoweb_clear_entities();
        module._isoweb_render();
        const baseline = readFrame();

        const createCharacter = (x: number, y: number, z: number, fx: number, fy: number) => {
          module._isoweb_clear_entities();
          const created = module.ccall(
            'isoweb_create_character',
            'number',
            ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
            ['diagnostic-character', 'demo', 'default', 'middle', x, y, z]
          );
          if (!created) throw new Error('unable to create diagnostic Character');
          module.ccall(
            'isoweb_set_character_hitbox',
            'number',
            ['string', 'number', 'number', 'number', 'number', 'number', 'number'],
            ['diagnostic-character', -0.28, -0.20, 0.0, 0.28, 0.20, 1.65]
          );
          module.ccall(
            'isoweb_set_character_forward',
            'number',
            ['string', 'number', 'number'],
            ['diagnostic-character', fx, fy]
          );
          module._isoweb_render();
          return readFrame();
        };

        const silhouette = (frame: Uint8ClampedArray) => {
          const width = canvas.width;
          const height = canvas.height;
          let pixels = 0;
          let minX = width;
          let maxX = -1;
          let minY = height;
          let maxY = -1;
          for (let pixel = 0; pixel < width * height; ++pixel) {
            const base = pixel * 4;
            const delta =
              Math.abs(frame[base] - baseline[base]) +
              Math.abs(frame[base + 1] - baseline[base + 1]) +
              Math.abs(frame[base + 2] - baseline[base + 2]);
            if (delta < 3) continue;
            const x = pixel % width;
            const y = Math.floor(pixel / width);
            ++pixels;
            minX = Math.min(minX, x);
            maxX = Math.max(maxX, x);
            minY = Math.min(minY, y);
            maxY = Math.max(maxY, y);
          }
          return pixels === 0
            ? { pixels: 0, width: 0, height: 0, minX: -1, maxX: -1, minY: -1, maxY: -1 }
            : {
                pixels,
                width: maxX - minX + 1,
                height: maxY - minY + 1,
                minX,
                maxX,
                minY,
                maxY
              };
        };

        const positions = [
          [-2.4, -2.4], [-2.4, 0.0], [-2.4, 2.4],
          [0.0, -2.4],                 [0.0, 2.4],
          [2.4, -2.4],  [2.4, 0.0],   [2.4, 2.4]
        ];
        const directions = [
          [0.0, 1.0],
          [1.0, 0.0],
          [0.0, -1.0],
          [-1.0, 0.0],
          [0.70710678, 0.70710678],
          [-0.70710678, 0.70710678],
          [0.70710678, -0.70710678],
          [-0.70710678, -0.70710678]
        ];

        const samples: any[] = [];
        for (const [x, y] of positions) {
          for (const [fx, fy] of directions) {
            const grounded = silhouette(createCharacter(x, y, 0.0, fx, fy));
            const lifted = silhouette(createCharacter(x, y, 2.4, fx, fy));
            const areaRatio = lifted.pixels > 0 ? grounded.pixels / lifted.pixels : null;
            const heightRatio = lifted.height > 0 ? grounded.height / lifted.height : null;
            samples.push({
              x, y, fx, fy,
              grounded,
              lifted,
              areaRatio,
              heightRatio
            });
          }
        }

        module._isoweb_clear_entities();
        module._isoweb_render();

        const visiblePairs = samples.filter(sample =>
          sample.grounded.pixels > 100 &&
          sample.lifted.pixels > 100 &&
          sample.grounded.minX > 1 &&
          sample.grounded.maxX < canvas.width - 2 &&
          sample.grounded.minY > 1 &&
          sample.grounded.maxY < canvas.height - 2 &&
          sample.lifted.minX > 1 &&
          sample.lifted.maxX < canvas.width - 2 &&
          sample.lifted.minY > 1 &&
          sample.lifted.maxY < canvas.height - 2
        );
        visiblePairs.sort((a, b) =>
          (a.areaRatio ?? 999) - (b.areaRatio ?? 999)
        );

        return {
          ok: true,
          display: 'webgl2',
          width: canvas.width,
          height: canvas.height,
          tested: samples.length,
          visible: visiblePairs.length,
          worst: visiblePairs.slice(0, 12)
        };
      });
    } finally {
      await silhouettePage.close();
    }
  })();
  console.log('[flat-floor-diagnostic] silhouette-scan', JSON.stringify(silhouetteScan));

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

  const flatFloorState = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    return {
      z: Number(module.ccall('isoweb_character_position_z', 'number', ['string'], ['demo-character'])),
      crouching: Number(module.ccall('isoweb_character_is_crouching', 'number', ['string'], ['demo-character']))
    };
  });
  if (Math.abs(flatFloorState.z) > 1.0e-5 || flatFloorState.crouching !== 0) {
    throw new Error(
      `Ordinary flat-floor movement changed Character height/posture unexpectedly: ${JSON.stringify(flatFloorState)}`
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

  console.log('Browser WASM boot, exact static cache reuse, JSON Character load, exact picking, real-surface movement, persistent selected redirects, centre joystick availability, yaw, detailed yaw, and detailed zoom smoke test passed.');
} finally {
  await browser.close();
  server.stop(true);
}
