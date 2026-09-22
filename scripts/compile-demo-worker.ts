import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';

import { CompiledPackageWriter } from '../web/src/world/CompiledPackageWriter';
import { PackageReader } from '../web/src/world/PackageReader';
import { PackageWriter } from '../web/src/world/PackageWriter';
import { WorldCompiler } from '../web/src/world/WorldCompiler';
import type { LevelDocument, WorldDocument } from '../web/src/world/documents';
import { validateLevelDocument, validateLoadedWorldPackage, validateWorldDocument } from '../web/src/world/validation';

const sourceRoot = 'web/worlds/demo';
const assetRoot = 'web/assets';

const world = validateWorldDocument(
  JSON.parse(await readFile(join(sourceRoot, 'world.json'), 'utf8')) as WorldDocument
);

const levels: LevelDocument[] = [];
for (const reference of world.levels) {
  const level = validateLevelDocument(
    JSON.parse(await readFile(join(sourceRoot, reference.path), 'utf8')) as LevelDocument
  );
  if (level.id !== reference.id) throw new Error(`Source level id mismatch: ${reference.id} != ${level.id}`);
  levels.push(level);
}

const sourceWriter = new PackageWriter();
const compiler = new WorldCompiler();
const compiledWriter = new CompiledPackageWriter();
const reader = new PackageReader();

const sourceManifest = {
  format: 'isoworld' as const,
  representation: 'source' as const,
  schemaVersion: 1,
  id: world.id,
  name: world.name,
  entry: 'world.json'
};
validateLoadedWorldPackage({ manifest: sourceManifest, world, levels });

await mkdir(assetRoot, { recursive: true });

// Preserve the editable/document path explicitly. The runtime can still open
// this package and will compile it in-browser through WorldCompiler.
const sourceWorldBytes = sourceWriter.writeWorldBytes(world, levels);
await writeFile(join(assetRoot, 'demo-source.isoworld'), sourceWorldBytes);

for (const level of levels) {
  await writeFile(
    join(assetRoot, `demo-${level.id}-source.isolevel`),
    sourceWriter.writeLevelBytes(level)
  );
}

// Deployment compilation is level-first on purpose: each authored level is
// lowered into the runtime IR, serialized as a real compiled .isolevel, and
// those exact package bytes are then nested into the compiled .isoworld.
const compiledLevels = levels.map(level => compiler.compileLevel(level));
const compiledLevelPackages = new Map<string, Uint8Array>();
for (const level of compiledLevels) {
  const bytes = compiledWriter.writeLevelBytes(level);
  const roundTrip = reader.loadLevelBytes(bytes);
  if (!('compiledFormatVersion' in roundTrip) || roundTrip.id !== level.id) {
    throw new Error(`Compiled isolevel round-trip failed for ${level.id}`);
  }
  compiledLevelPackages.set(level.id, bytes);
  await writeFile(join(assetRoot, `demo-${level.id}.isolevel`), bytes);
}

const compiledPaths = new Map(
  world.levels.map(reference => [reference.id, `levels/${reference.id}.isolevel`])
);
const compiledWorld = compiler.compileWorld(world, compiledPaths);
const compiledWorldBytes = compiledWriter.writeWorldBytes(
  compiledWorld,
  compiledLevels,
  compiledLevelPackages
);
const compiledRoundTrip = reader.loadWorldBytes(compiledWorldBytes);
if (
  compiledRoundTrip.manifest.representation !== 'compiled' ||
  compiledRoundTrip.world.id !== world.id ||
  compiledRoundTrip.levels.length !== levels.length
) {
  throw new Error('Compiled isoworld round-trip failed');
}

// Canonical demo artifact is compiled. The source package remains available as
// demo-source.isoworld for editor/dev/runtime compatibility.
await writeFile(join(assetRoot, 'demo.isoworld'), compiledWorldBytes);

console.log(
  `Compiled demo deployment: ${compiledLevels.length} isolevel packages -> demo.isoworld; ` +
  'source package retained as demo-source.isoworld.'
);
