import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';

import { CompiledPackageWriter } from '../web/src/world/CompiledPackageWriter';
import {
  inferAssetMediaType, type PackageAssetInput, type PackageAssetInputMap
} from '../web/src/world/PackageAssets';
import { PackageReader } from '../web/src/world/PackageReader';
import { PackageWriter } from '../web/src/world/PackageWriter';
import { WorldCompiler } from '../web/src/world/WorldCompiler';
import type {
  AssetSourceDefinition, LevelDocument, WorldDocument
} from '../web/src/world/documents';
import {
  validateLevelDocument, validateWorldDocument
} from '../web/src/world/validation';

const sourceRoot = 'web/worlds/demo';
const assetRoot = 'web/assets';

function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.byteLength !== b.byteLength) return false;
  for (let index = 0; index < a.byteLength; ++index) {
    if (a[index] !== b[index]) return false;
  }
  return true;
}

async function loadDeclaredAssets(
  baseDirectory: string,
  definitions: Record<string, AssetSourceDefinition> | undefined
): Promise<Map<string, PackageAssetInput>> {
  const result = new Map<string, PackageAssetInput>();
  for (const [id, definition] of Object.entries(definitions ?? {})) {
    const bytes = new Uint8Array(await readFile(join(baseDirectory, definition.source)));
    result.set(id, {
      bytes,
      mediaType: inferAssetMediaType(id, definition.mediaType)
    });
  }
  return result;
}

function mergeAssetInputs(
  target: Map<string, PackageAssetInput>,
  incoming: PackageAssetInputMap,
  context: string
): void {
  const entries = incoming instanceof Map ? incoming : new Map(Object.entries(incoming));
  for (const [id, asset] of entries) {
    const existing = target.get(id);
    if (!existing) {
      target.set(id, asset);
      continue;
    }
    if (
      inferAssetMediaType(id, existing.mediaType) !== inferAssetMediaType(id, asset.mediaType) ||
      !sameBytes(existing.bytes, asset.bytes)
    ) {
      throw new Error(`Asset id ${id} has conflicting bytes while assembling ${context}`);
    }
  }
}

const world = validateWorldDocument(
  JSON.parse(await readFile(join(sourceRoot, 'world.json'), 'utf8')) as WorldDocument
);

const levels: LevelDocument[] = [];
const levelAssets = new Map<string, Map<string, PackageAssetInput>>();
for (const reference of world.levels) {
  const sourcePath = join(sourceRoot, reference.path);
  const level = validateLevelDocument(
    JSON.parse(await readFile(sourcePath, 'utf8')) as LevelDocument
  );
  if (level.id !== reference.id) {
    throw new Error(`Source level id mismatch: ${reference.id} != ${level.id}`);
  }
  levels.push(level);
  levelAssets.set(
    level.id,
    await loadDeclaredAssets(dirname(sourcePath), level.assets)
  );
}

const worldAssets = await loadDeclaredAssets(sourceRoot, world.assets);
const completeSourceWorldAssets = new Map<string, PackageAssetInput>(worldAssets);
for (const level of levels) {
  mergeAssetInputs(
    completeSourceWorldAssets,
    levelAssets.get(level.id) ?? new Map(),
    `source world ${world.id}`
  );
}

const sourceWriter = new PackageWriter();
const compiler = new WorldCompiler();
const compiledWriter = new CompiledPackageWriter();
const reader = new PackageReader();

await mkdir(assetRoot, { recursive: true });

const sourceWorldBytes = sourceWriter.writeWorldBytes(
  world,
  levels,
  completeSourceWorldAssets
);
await writeFile(join(assetRoot, 'demo-source.isoworld'), sourceWorldBytes);

for (const level of levels) {
  await writeFile(
    join(assetRoot, `demo-${level.id}-source.isolevel`),
    sourceWriter.writeLevelBytes(level, levelAssets.get(level.id))
  );
}

const compiledLevels = levels.map(level => compiler.compileLevel(level));
const compiledLevelPackages = new Map<string, Uint8Array>();
for (const level of compiledLevels) {
  const assets = levelAssets.get(level.id);
  const bytes = compiledWriter.writeLevelBytes(level, assets);
  const roundTrip = reader.loadLevelPackageBytes(bytes);
  if (
    !('compiledFormatVersion' in roundTrip.level) ||
    roundTrip.level.id !== level.id
  ) {
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
  compiledLevelPackages,
  worldAssets
);
const compiledRoundTrip = reader.loadWorldBytes(compiledWorldBytes);
if (
  compiledRoundTrip.manifest.representation !== 'compiled' ||
  compiledRoundTrip.world.id !== world.id ||
  compiledRoundTrip.levels.length !== levels.length
) {
  throw new Error('Compiled isoworld round-trip failed');
}

await writeFile(join(assetRoot, 'demo.isoworld'), compiledWorldBytes);

console.log(
  `Compiled self-contained demo deployment: ${compiledLevels.length} isolevel packages -> demo.isoworld; ` +
  'source world and standalone source/compiled levels retain embedded assets.'
);
