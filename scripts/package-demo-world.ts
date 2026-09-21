import { mkdir, readdir, readFile, writeFile } from 'node:fs/promises';
import { join, relative } from 'node:path';
import { strToU8, zipSync } from '../vendor/fflate/index';

const sourceRoot = 'web/worlds/demo';
const assetRoot = 'web/assets';

async function collect(root: string, directory = root): Promise<Record<string, Uint8Array>> {
  const files: Record<string, Uint8Array> = {};
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) Object.assign(files, await collect(root, path));
    else if (entry.isFile()) {
      const name = relative(root, path).split('\\').join('/');
      files[name] = new Uint8Array(await readFile(path));
    }
  }
  return files;
}

await mkdir(assetRoot, { recursive: true });
await writeFile(join(assetRoot, 'demo.isoworld'), zipSync(await collect(sourceRoot), { level: 6 }));

const world = JSON.parse(await readFile(join(sourceRoot, 'world.json'), 'utf8')) as {
  levels: Array<{ id: string; path: string }>;
};
for (const reference of world.levels) {
  const levelBytes = await readFile(join(sourceRoot, reference.path));
  const level = JSON.parse(levelBytes.toString('utf8')) as { id: string; name?: string };
  const manifest = {
    format: 'isolevel', schemaVersion: 1, id: level.id, name: level.name, entry: 'level.json',
    createdWith: { application: 'isoweb-level-builder', version: '0.1.0' }
  };
  await writeFile(
    join(assetRoot, `demo-${reference.id}.isolevel`),
    zipSync({
      'manifest.json': strToU8(JSON.stringify(manifest, null, 2) + '\n'),
      'level.json': new Uint8Array(levelBytes)
    }, { level: 6 })
  );
}
