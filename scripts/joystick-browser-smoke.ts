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

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
const errors: string[] = [];
page.on('pageerror', error => errors.push(error.stack || error.message));

async function status(): Promise<string> {
  return (await page.locator('#view-status').textContent()) ?? '';
}

async function drag(id: string, dx: number, dy: number, holdMs = 320): Promise<void> {
  const box = await page.locator(id).boundingBox();
  if (!box) throw new Error(`${id} has no bounding box.`);
  const x = box.x + box.width * 0.5;
  const y = box.y + box.height * 0.5;
  await page.mouse.move(x, y);
  await page.mouse.down();
  await page.mouse.move(x + dx, y + dy, { steps: 4 });
  await page.waitForTimeout(holdMs);
  await page.mouse.up();
  await page.waitForTimeout(100);
}

try {
  await page.goto(`http://127.0.0.1:${server.port}/`, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(
    () => document.documentElement.classList.contains('wasm-ready'),
    undefined,
    { timeout: 45_000 }
  );

  console.log('[joystick-browser] browser zoom gestures stay trapped in the game viewport');
  const gesturePolicy = await page.evaluate(() => {
    const viewport = document.getElementById('viewport') as HTMLElement | null;
    const resetCamera = document.getElementById('reset-camera') as HTMLButtonElement | null;
    const meta = document.querySelector('meta[name="viewport"]')?.getAttribute('content') ?? '';
    if (!viewport || !resetCamera) return null;

    const doubleClick = new MouseEvent('dblclick', { bubbles: true, cancelable: true });
    const doubleClickAllowed = viewport.dispatchEvent(doubleClick);
    const sceneTouchEnd = new Event('touchend', { bubbles: true, cancelable: true });
    const sceneTouchEndAllowed = viewport.dispatchEvent(sceneTouchEnd);
    const controlTouchEnd = new Event('touchend', { bubbles: true, cancelable: true });
    const controlTouchEndAllowed = resetCamera.dispatchEvent(controlTouchEnd);

    return {
      meta,
      viewportTouchAction: getComputedStyle(viewport).touchAction,
      bodyTouchAction: getComputedStyle(document.body).touchAction,
      controlTouchAction: getComputedStyle(resetCamera).touchAction,
      doubleClickAllowed,
      sceneTouchEndAllowed,
      controlTouchEndAllowed
    };
  });
  if (!gesturePolicy) throw new Error('Gesture policy elements are missing.');
  if (!gesturePolicy.meta.includes('maximum-scale=1') || !gesturePolicy.meta.includes('user-scalable=no')) {
    throw new Error(`Viewport zoom lock regressed: ${gesturePolicy.meta}`);
  }
  if (
    gesturePolicy.viewportTouchAction !== 'none' ||
    gesturePolicy.bodyTouchAction !== 'none' ||
    gesturePolicy.controlTouchAction !== 'none'
  ) {
    throw new Error(`Native touch-action escaped the game: ${JSON.stringify(gesturePolicy)}`);
  }
  if (gesturePolicy.doubleClickAllowed) throw new Error('Viewport dblclick default was not cancelled.');
  if (gesturePolicy.sceneTouchEndAllowed) throw new Error('Scene touchend default was not cancelled.');
  if (!gesturePolicy.controlTouchEndAllowed) {
    throw new Error('Single control touchend was unnecessarily cancelled, risking lost button clicks.');
  }

  const layout = await page.evaluate(() => {
    const ids = [
      'reset-level', 'level-up', 'level-down',
      'reset-zoom', 'zoom-in', 'zoom-out',
      'reset-yaw', 'reset-camera'
    ];
    return ids.map(id => {
      const element = document.getElementById(id) as HTMLButtonElement | null;
      if (!element) return null;
      const rect = element.getBoundingClientRect();
      return {
        id,
        left: rect.left,
        top: rect.top,
        width: rect.width,
        height: rect.height,
        centreX: rect.left + rect.width * 0.5,
        centreY: rect.top + rect.height * 0.5,
        disabled: element.disabled,
        joystick: element.dataset.joystick
      };
    });
  });

  if (layout.some(value => !value)) throw new Error('A controller hitbox is missing.');
  for (const id of ['reset-level', 'reset-zoom', 'reset-yaw', 'reset-camera']) {
    const value = layout.find(item => item?.id === id)!;
    if (value.disabled) throw new Error(`${id} is disabled and cannot act as a joystick.`);
    if (value.joystick !== 'true') throw new Error(`${id} is not marked as a joystick.`);
  }

  const level = layout.find(value => value?.id === 'reset-level')!;
  const levelUp = layout.find(value => value?.id === 'level-up')!;
  const levelDown = layout.find(value => value?.id === 'level-down')!;
  const zoom = layout.find(value => value?.id === 'reset-zoom')!;
  const zoomIn = layout.find(value => value?.id === 'zoom-in')!;
  const zoomOut = layout.find(value => value?.id === 'zoom-out')!;

  if (!(zoom.left < level.left)) throw new Error('Zoom must remain on the left and Z-level on the right.');
  if (!(zoomOut.centreX < zoom.centreX && zoom.centreX < zoomIn.centreX)) {
    throw new Error('Zoom controls are not arranged horizontally as minus, centre, plus.');
  }
  if (Math.abs(zoomOut.centreY - zoom.centreY) > 3 || Math.abs(zoomIn.centreY - zoom.centreY) > 3) {
    throw new Error('Zoom minus/centre/plus are not on one horizontal row.');
  }
  if (!(levelUp.centreY < level.centreY && level.centreY < levelDown.centreY)) {
    throw new Error('Z-level up/centre/down are not arranged vertically.');
  }

  console.log('[joystick-browser] tap reset remains functional');
  await page.locator('#rotate-clockwise').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 90 degrees'));
  await page.locator('#reset-yaw').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 0 degrees'));

  console.log('[joystick-browser] pan centre disc continuously without rebuilding static world');
  const buildsBeforePan = await page.evaluate(() => (globalThis as any).Module._isoweb_static_cache_build_count());
  await drag('#reset-camera', 32, 0, 360);
  const pannedStatus = await status();
  const panMatch = pannedStatus.match(/pan X (-?\d+(?:\.\d+)?); Y (-?\d+(?:\.\d+)?)/);
  if (!panMatch || (Math.abs(Number(panMatch[1])) < 0.01 && Math.abs(Number(panMatch[2])) < 0.01)) {
    throw new Error(`Pan joystick did not move the camera: ${pannedStatus}`);
  }
  const buildsAfterPan = await page.evaluate(() => (globalThis as any).Module._isoweb_static_cache_build_count());
  if (buildsAfterPan !== buildsBeforePan) {
    throw new Error(`Continuous pan rebuilt the full static world: ${buildsBeforePan} -> ${buildsAfterPan}`);
  }
  await page.locator('#reset-camera').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('pan X 0.00; Y 0.00'));

  console.log('[joystick-browser] portrait pan centre disc reaches beyond the centre room');
  await drag('#reset-camera', 0, 56, 3200);
  const farPanStatus = await status();
  const farPanMatch = farPanStatus.match(/pan X (-?\d+(?:\.\d+)?); Y (-?\d+(?:\.\d+)?)/);
  if (!farPanMatch) throw new Error(`Could not read far pan status: ${farPanStatus}`);
  const farPanDistance = Math.hypot(Number(farPanMatch[1]), Number(farPanMatch[2]));
  if (farPanDistance <= 8.0) {
    throw new Error(`Centre pan joystick still stopped around the middle room: ${farPanStatus}`);
  }
  await page.locator('#reset-camera').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('pan X 0.00; Y 0.00'));

  console.log('[joystick-browser] yaw centre disc');
  await drag('#reset-yaw', 32, 0);
  if ((await status()).includes('Camera 0 degrees')) throw new Error('Yaw joystick did not rotate the camera.');
  await page.locator('#reset-yaw').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('Camera 0 degrees'));

  console.log('[joystick-browser] zoom centre disc');
  await drag('#reset-zoom', 32, 0);
  if ((await status()).includes('zoom 1x')) throw new Error('Zoom joystick did not change zoom.');
  await page.locator('#reset-zoom').click();
  await page.waitForFunction(() => document.getElementById('view-status')?.textContent?.includes('zoom 1x'));

  console.log('[joystick-browser] Z-level centre disc');
  const initialLevel = await page.evaluate(() => (globalThis as any).Module._isoweb_active_level_index());
  await drag('#reset-level', 0, -32);
  const raisedLevel = await page.evaluate(() => (globalThis as any).Module._isoweb_active_level_index());
  if (!(raisedLevel > initialLevel)) {
    throw new Error(`Level joystick did not move upward: ${initialLevel} -> ${raisedLevel}`);
  }
  await page.locator('#reset-level').click();
  await page.waitForFunction(
    () => (globalThis as any).Module._isoweb_active_level_index() ===
      (globalThis as any).Module._isoweb_default_level_index()
  );

  console.log('[joystick-browser] z=0 floor remains identical after pan-cache reuse');
  await page.evaluate(() => {
    const module = (globalThis as any).Module;
    module.ccall('isoweb_clear_entities', null, [], []);
    module.ccall('isoweb_level_down', null, [], []);
    const created = module.ccall(
      'isoweb_create_character',
      'number',
      ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
      ['depth-probe', 'demo', 'default', 'lower', 0.0, 2.4, 0.0]
    );
    if (created !== 1) throw new Error('Could not create z=0 depth-probe Character.');
    module.ccall('isoweb_render', null, [], []);
  });

  const shiftedFrame = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) throw new Error('Canvas missing.');

    const buildsBefore = module._isoweb_static_cache_build_count();
    module.ccall(
      'isoweb_pan',
      null,
      ['number', 'number'],
      [0.0, 1.35]
    );
    const buildsAfter = module._isoweb_static_cache_build_count();

    const gl = canvas.getContext('webgl2');
    let pixels: Uint8Array;
    if (gl) {
      pixels = new Uint8Array(canvas.width * canvas.height * 4);
      gl.readPixels(0, 0, canvas.width, canvas.height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    } else {
      const context = canvas.getContext('2d');
      if (!context) throw new Error('No readable canvas context.');
      pixels = new Uint8Array(context.getImageData(0, 0, canvas.width, canvas.height).data);
    }

    (globalThis as any).__isowebShiftedDepthFrame = pixels;
    return {
      width: canvas.width,
      height: canvas.height,
      buildsBefore,
      buildsAfter
    };
  });

  if (shiftedFrame.buildsAfter !== shiftedFrame.buildsBefore) {
    throw new Error(
      `Depth regression setup unexpectedly rebuilt static cache: ${shiftedFrame.buildsBefore} -> ${shiftedFrame.buildsAfter}`
    );
  }

  const depthParity = await page.evaluate(() => {
    const module = (globalThis as any).Module;
    const canvas = document.getElementById('canvas') as HTMLCanvasElement | null;
    if (!canvas) throw new Error('Canvas missing.');
    module._isoweb_set_render_thread_limit(1);
    module._isoweb_render();

    const gl = canvas.getContext('webgl2');
    let rebuilt: Uint8Array;
    if (gl) {
      rebuilt = new Uint8Array(canvas.width * canvas.height * 4);
      gl.readPixels(0, 0, canvas.width, canvas.height, gl.RGBA, gl.UNSIGNED_BYTE, rebuilt);
    } else {
      const context = canvas.getContext('2d');
      if (!context) throw new Error('No readable canvas context.');
      rebuilt = new Uint8Array(context.getImageData(0, 0, canvas.width, canvas.height).data);
    }

    const shifted = (globalThis as any).__isowebShiftedDepthFrame as Uint8Array;
    let differingBytes = 0;
    let differingPixels = 0;
    let minX = canvas.width;
    let minY = canvas.height;
    let maxX = -1;
    let maxY = -1;
    for (let offset = 0; offset < rebuilt.length; offset += 4) {
      let pixelDiffers = false;
      for (let channel = 0; channel < 4; ++channel) {
        if (shifted[offset + channel] !== rebuilt[offset + channel]) {
          ++differingBytes;
          pixelDiffers = true;
        }
      }
      if (!pixelDiffers) continue;
      ++differingPixels;
      const pixel = offset >> 2;
      const x = pixel % canvas.width;
      const y = Math.floor(pixel / canvas.width);
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
    return {
      differingBytes,
      differingPixels,
      minX,
      minY,
      maxX,
      maxY,
      width: canvas.width,
      height: canvas.height
    };
  });

  if (depthParity.differingPixels !== 0) {
    throw new Error(
      `A pan-shifted z=0 scene differs from a full rebuild at the same camera position: ${JSON.stringify(depthParity)}`
    );
  }
  if (shiftedFrame.width <= 0 || shiftedFrame.height <= 0) {
    throw new Error('Depth parity frame was empty.');
  }

  if (errors.length) throw new Error(errors.join('\n\n'));
  console.log('Centre joystick browser smoke passed: page gestures locked, full portrait pan range, tap resets, pan/yaw/zoom/Z drags, pan cache reuse, and shifted-cache/full-rebuild depth parity.');
} finally {
  await browser.close();
  server.stop(true);
}
