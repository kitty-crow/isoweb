import { strToU8, zipSync } from '../vendor/fflate/index';
import { PackageReader } from '../web/src/world/PackageReader';
import { PackageWriter } from '../web/src/world/PackageWriter';
import type { LevelDocument, WorldDocument } from '../web/src/world/documents';

const root = 'web/worlds/demo';
const world = await Bun.file(`${root}/world.json`).json() as WorldDocument;
const levels: LevelDocument[] = [];
for (const reference of world.levels) {
  levels.push(await Bun.file(`${root}/${reference.path}`).json() as LevelDocument);
}

const writer = new PackageWriter();
const loaded = new PackageReader().loadWorldBytes(writer.writeWorldBytes(world, levels));
if (
  loaded.world.id !== 'demo' || loaded.levels.length !== 3 ||
  loaded.world.settings.defaultLevel !== 'middle' ||
  (loaded.world.connectors ?? []).length !== 2 ||
  loaded.levels.map(level => level.id).join(',') !== 'lower,middle,upper'
) throw new Error('World package round-trip changed the demo');

for (const level of levels) {
  if (writer.writeLevelBytes(level).byteLength < 100) throw new Error(`Empty isolevel ${level.id}`);
}

const malicious = zipSync({ '../outside.json': strToU8('{}'), 'manifest.json': strToU8('{}') });
let rejected = false;
try { new PackageReader().loadWorldBytes(malicious); } catch { rejected = true; }
if (!rejected) throw new Error('Package reader accepted path traversal');

console.log('World package smoke passed: three levels round-trip and unsafe ZIP paths are rejected.');
