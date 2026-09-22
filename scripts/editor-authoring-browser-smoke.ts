import { createRequire } from 'node:module';
import { extname, normalize } from 'node:path';

const require = createRequire(import.meta.url);
const { chromium } = require('../vendor/pages/node_modules/playwright');

const mimeTypes: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8'
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
    const file = Bun.file(`site/${relative}`);
    if (!(await file.exists())) return new Response('Not found', { status: 404 });
    return new Response(file, {
      headers: { 'Content-Type': mimeTypes[extname(relative)] ?? 'application/octet-stream' }
    });
  }
});

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
const errors: string[] = [];
page.on('pageerror', error => errors.push(error.stack || error.message));

try {
  await page.goto(`http://127.0.0.1:${server.port}/level-builder/`, {
    waitUntil: 'domcontentloaded'
  });
  await page.waitForSelector('#editor-layout');

  const paletteCount = await page.locator('[data-editor-add-kind]').count();
  if (paletteCount < 12) throw new Error(`Builder palette is incomplete: ${paletteCount} items`);

  const layout = page.locator('#editor-layout');
  const layoutBox = await layout.boundingBox();
  if (!layoutBox) throw new Error('Builder layout has no bounds');

  await page.locator('[data-editor-add-kind="cube"]').click();
  await page.mouse.click(
    layoutBox.x + layoutBox.width / 2,
    layoutBox.y + layoutBox.height / 2
  );
  const cube = page.locator('.editor-layout-item[data-kind="geometry"][data-selected="true"]');
  await cube.waitFor();
  const cubeBefore = await cube.boundingBox();
  if (!cubeBefore) throw new Error('New cube has no layout bounds');

  await page.mouse.move(cubeBefore.x + cubeBefore.width / 2, cubeBefore.y + cubeBefore.height / 2);
  await page.mouse.down();
  await page.mouse.move(
    cubeBefore.x + cubeBefore.width / 2 + 64,
    cubeBefore.y + cubeBefore.height / 2 - 32,
    { steps: 5 }
  );
  await page.mouse.up();

  const movedCube = page.locator('.editor-layout-item[data-kind="geometry"][data-selected="true"]');
  const cubeAfter = await movedCube.boundingBox();
  if (!cubeAfter || cubeAfter.x < cubeBefore.x + 45 || cubeAfter.y > cubeBefore.y - 15) {
    throw new Error(
      `Direct drag did not reposition cube as expected: before=${JSON.stringify(cubeBefore)} after=${JSON.stringify(cubeAfter)}`
    );
  }

  const resize = movedCube.locator('.editor-layout-resize');
  const resizeBox = await resize.boundingBox();
  if (!resizeBox) throw new Error('Selected cube has no resize handle');
  const widthBeforeResize = cubeAfter.width;
  await page.mouse.move(resizeBox.x + resizeBox.width / 2, resizeBox.y + resizeBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(resizeBox.x + resizeBox.width / 2 + 32, resizeBox.y + resizeBox.height / 2 + 32, {
    steps: 4
  });
  await page.mouse.up();

  const resizedBox = await movedCube.boundingBox();
  if (!resizedBox || resizedBox.width <= widthBeforeResize + 10) {
    throw new Error('Resize handle did not expand selected geometry');
  }

  const undo = page.locator('#editor-undo');
  if (await undo.isDisabled()) throw new Error('Direct manipulation did not create undo history');
  await undo.click();
  const undoneBox = await movedCube.boundingBox();
  if (!undoneBox || Math.abs(undoneBox.width - widthBeforeResize) > 3) {
    throw new Error('Undo did not restore the pre-resize geometry size');
  }

  await page.dragAndDrop(
    '[data-editor-add-kind="room"]',
    '#editor-layout',
    {
      targetPosition: {
        x: Math.round(layoutBox.width * 0.68),
        y: Math.round(layoutBox.height * 0.38)
      }
    }
  );
  const rooms = page.locator('.editor-layout-item[data-kind="room"]');
  if (await rooms.count() < 1) throw new Error('Palette drag/drop did not create a room');

  await page.dragAndDrop(
    '[data-editor-add-kind="floor-hole"]',
    '#editor-layout',
    {
      targetPosition: {
        x: Math.round(layoutBox.width * 0.36),
        y: Math.round(layoutBox.height * 0.32)
      }
    }
  );
  const floorHole = page.locator('.editor-layout-item[data-kind="floor-hole"][data-selected="true"]');
  await floorHole.waitFor();
  if (await floorHole.locator('.editor-layout-resize').count() !== 1) {
    throw new Error('Selected floor hole does not expose a resize handle');
  }

  await page.dragAndDrop(
    '[data-editor-add-kind="staircase"]',
    '#editor-layout',
    {
      targetPosition: {
        x: Math.round(layoutBox.width * 0.58),
        y: Math.round(layoutBox.height * 0.68)
      }
    }
  );
  const staircase = page.locator('.editor-layout-item[data-kind="staircase"][data-selected="true"]');
  await staircase.waitFor();
  if (await staircase.locator('.editor-layout-resize').count() !== 1) {
    throw new Error('Selected staircase does not expose a resize handle');
  }

  await page.locator('[data-editor-add-kind="character"]').click();
  await page.mouse.click(
    layoutBox.x + layoutBox.width * 0.42,
    layoutBox.y + layoutBox.height * 0.62
  );
  const character = page.locator('.editor-layout-item[data-kind="entity"][data-selected="true"]');
  await character.waitFor();
  if (await character.locator('.editor-layout-rotate').count() !== 1) {
    throw new Error('Selected character does not expose a facing/rotation handle');
  }

  await page.setViewportSize({ width: 844, height: 390 });
  await page.waitForTimeout(100);
  const overflow = await page.evaluate(() => ({
    viewport: innerWidth,
    documentWidth: Math.max(document.documentElement.scrollWidth, document.body.scrollWidth),
    bodyOverflowX: getComputedStyle(document.body).overflowX
  }));
  if (overflow.bodyOverflowX === 'hidden' || overflow.bodyOverflowX === 'clip' ||
      overflow.documentWidth > overflow.viewport + 1) {
    throw new Error(`Builder landscape layout is not responsive: ${JSON.stringify(overflow)}`);
  }

  if (errors.length > 0) throw new Error(`Browser errors:\n${errors.join('\n')}`);
  console.log('Graphical Level Builder browser smoke passed: palette placement, rooms, floor holes, stairs, direct move/resize, facing rotation, undo and landscape layout work.');
} finally {
  await browser.close();
  await server.stop(true);
}
