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
  await page.goto(`http://127.0.0.1:${server.port}/world-editor/`, {
    waitUntil: 'domcontentloaded'
  });
  await page.waitForSelector('#world-composition');

  if (await page.locator('[data-editor-add-kind]').count() !== 0 ||
      await page.locator('#editor-add-kind').count() !== 0 ||
      await page.locator('#editor-add').count() !== 0) {
    throw new Error('World Editor still exposes Level Builder object-authoring controls');
  }

  for (const selector of [
    '#world-level-import',
    '#world-stack-above',
    '#world-stack-below',
    '#world-rotate-left',
    '#world-rotate-right',
    '#world-connector-type',
    '#world-connector-add'
  ]) {
    if (await page.locator(selector).count() !== 1) {
      throw new Error(`World composition control is missing: ${selector}`);
    }
  }

  if (await page.locator('.world-level-card').count() !== 1) {
    throw new Error('New world did not start with one level card');
  }

  await page.locator('#world-level-duplicate').click();
  await page.waitForFunction(() => document.querySelectorAll('.world-level-card').length === 2);

  const selected = page.locator('.world-level-card[data-selected="true"]');
  await selected.waitFor();
  await page.locator('#world-rotate-left').click();
  await page.waitForFunction(() => {
    const card = document.querySelector('.world-level-card[data-selected="true"]');
    return card?.textContent?.includes('90°') ?? false;
  });

  await page.locator('#world-stack-gap').fill('3.5');
  await page.locator('#world-stack-above').click();
  await page.waitForFunction(() => {
    const card = document.querySelector('.world-level-card[data-selected="true"]');
    return card?.textContent?.includes('Z 3.50 m') ?? false;
  });

  await page.locator('#world-connector-type').selectOption('stairs');
  await page.locator('#world-connector-add').click();
  await page.waitForFunction(() =>
    Array.from(document.querySelectorAll('#world-connection-list .world-list-item'))
      .some(item => item.textContent?.includes('Stairs'))
  );

  const cards = page.locator('.world-level-card');
  await cards.nth(1).click();
  const before = await page.locator('.world-level-card[data-selected="true"]').boundingBox();
  if (!before) throw new Error('Selected world level has no composition bounds');

  await page.mouse.move(before.x + before.width / 2, before.y + before.height / 2);
  await page.mouse.down();
  await page.mouse.move(
    before.x + before.width / 2 + 60,
    before.y + before.height / 2 - 30,
    { steps: 5 }
  );
  await page.mouse.up();

  const movedMeta = await page.locator('#world-level-list .world-list-item[data-selected="true"] span').textContent();
  if (!movedMeta || movedMeta.includes('X 0.0 · Y 0.0')) {
    throw new Error(`Dragging a whole level did not change world placement: ${movedMeta}`);
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
    throw new Error(`World Editor landscape layout is not responsive: ${JSON.stringify(overflow)}`);
  }

  if (errors.length > 0) throw new Error(`Browser errors:\n${errors.join('\n')}`);
  console.log('World Editor browser smoke passed: level-only composition UI, whole-level rotate/stack/drag and inter-level stairs work.');
} finally {
  await browser.close();
  await server.stop(true);
}
