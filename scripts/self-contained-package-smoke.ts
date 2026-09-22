import { readFile, writeFile } from 'node:fs/promises';
import { strFromU8, unzipSync } from '../vendor/fflate/index';

import { CompiledPackageWriter } from '../web/src/world/CompiledPackageWriter';
import { packageAssetPath } from '../web/src/world/PackageAssets';
import { PackageReader } from '../web/src/world/PackageReader';
import { PackageWriter } from '../web/src/world/PackageWriter';
import { WorldCompiler } from '../web/src/world/WorldCompiler';
import type { LevelDocument, PackageManifest, WorldDocument } from '../web/src/world/documents';
import { validateLevelDocument, validateWorldDocument } from '../web/src/world/validation';

const clone = <T>(value: T): T => JSON.parse(JSON.stringify(value));

const originalWorld = validateWorldDocument(
  JSON.parse(await readFile('web/worlds/demo/world.json', 'utf8')) as WorldDocument
);
const originalMiddle = validateLevelDocument(
  JSON.parse(await readFile('web/worlds/demo/levels/middle.json', 'utf8')) as LevelDocument
);

const level = clone(originalMiddle);
level.assets = {
  'probe.png': { source: 'probe.png', mediaType: 'image/png' }
};
const character = level.entities.find(entity => entity.id === 'demo-character');
if (!character || !('character' in character.components)) throw new Error('Demo Character fixture missing');
character.components.character.sprites = {
  still: {
    front: {
      resource: 'probe.png',
      frameCount: 1,
      columns: 1,
      rows: 1,
      fps: 1,
      worldWidth: 1,
      worldHeight: 1,
      loop: true
    }
  }
};
validateLevelDocument(level);

const world = clone(originalWorld);
world.levels = [{ id: 'middle', path: 'levels/middle.json' }];
world.settings.defaultLevel = 'middle';
world.connectors = [];
world.behaviours = [];
world.assets = {
  'world-probe.bin': {
    source: 'world-probe.bin',
    mediaType: 'application/octet-stream'
  }
};
validateWorldDocument(world);

const probePng = Uint8Array.from(
  Buffer.from(
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=',
    'base64'
  )
);
const worldProbe = new Uint8Array([0x49, 0x53, 0x4f, 0x57, 0x45, 0x42]);
const levelAssets = new Map([
  ['probe.png', { bytes: probePng, mediaType: 'image/png' }]
]);
const allSourceAssets = new Map([
  ...levelAssets,
  ['world-probe.bin', { bytes: worldProbe, mediaType: 'application/octet-stream' }]
]);

const sourceWriter = new PackageWriter();
const compiledWriter = new CompiledPackageWriter();
const compiler = new WorldCompiler();
const reader = new PackageReader();

function mustThrow(label: string, callback: () => unknown): void {
  let threw = false;
  try { callback(); } catch { threw = true; }
  if (!threw) throw new Error(label + ' unexpectedly accepted a missing asset');
}

function assertBytes(label: string, actual: Uint8Array | undefined, expected: Uint8Array): void {
  if (!actual || actual.byteLength !== expected.byteLength) throw new Error(label + ' byte length mismatch');
  for (let index = 0; index < expected.byteLength; ++index) {
    if (actual[index] !== expected[index]) throw new Error(label + ' byte mismatch at ' + index);
  }
}

mustThrow('source isolevel', () => sourceWriter.writeLevelBytes(level));
mustThrow('source isoworld', () => sourceWriter.writeWorldBytes(world, [level], new Map([
  ['world-probe.bin', { bytes: worldProbe }]
])));

const sourceLevelBytes = sourceWriter.writeLevelBytes(level, levelAssets);
const sourceLevelPackage = reader.loadLevelPackageBytes(sourceLevelBytes);
assertBytes('source isolevel embedded sprite', sourceLevelPackage.assets.get('probe.png')?.bytes, probePng);

const sourceWorldBytes = sourceWriter.writeWorldBytes(world, [level], allSourceAssets);
const sourceWorldPackage = reader.loadWorldBytes(sourceWorldBytes);
assertBytes('source isoworld embedded sprite', sourceWorldPackage.assets.get('probe.png')?.bytes, probePng);
assertBytes('source isoworld embedded world asset', sourceWorldPackage.assets.get('world-probe.bin')?.bytes, worldProbe);

const compiledLevel = compiler.compileLevel(level);
mustThrow('compiled isolevel', () => compiledWriter.writeLevelBytes(compiledLevel));
const compiledLevelBytes = compiledWriter.writeLevelBytes(compiledLevel, levelAssets);
const compiledLevelPackage = reader.loadLevelPackageBytes(compiledLevelBytes);
assertBytes('compiled isolevel embedded sprite', compiledLevelPackage.assets.get('probe.png')?.bytes, probePng);

const compiledWorld = compiler.compileWorld(
  world,
  new Map([['middle', 'levels/middle.isolevel']])
);
const compiledWorldBytes = compiledWriter.writeWorldBytes(
  compiledWorld,
  [compiledLevel],
  new Map([['middle', compiledLevelBytes]]),
  new Map([['world-probe.bin', { bytes: worldProbe, mediaType: 'application/octet-stream' }]])
);
const compiledWorldPackage = reader.loadWorldBytes(compiledWorldBytes);
await writeFile('web/assets/self-contained-fixture.isoworld', compiledWorldBytes);
assertBytes('compiled isoworld nested sprite', compiledWorldPackage.assets.get('probe.png')?.bytes, probePng);
assertBytes('compiled isoworld world asset', compiledWorldPackage.assets.get('world-probe.bin')?.bytes, worldProbe);

const compiledArchive = unzipSync(compiledWorldBytes);
const compiledManifest = JSON.parse(strFromU8(compiledArchive['manifest.json'])) as PackageManifest;
if (!compiledManifest.assets?.some(asset => asset.id === 'world-probe.bin')) {
  throw new Error('Compiled isoworld manifest does not list its embedded world asset');
}
const nestedBytes = compiledArchive['levels/middle.isolevel'];
if (!nestedBytes) throw new Error('Compiled isoworld does not physically contain middle.isolevel');
const nestedArchive = unzipSync(nestedBytes);
const nestedManifest = JSON.parse(strFromU8(nestedArchive['manifest.json'])) as PackageManifest;
if (!nestedManifest.assets?.some(asset => asset.id === 'probe.png')) {
  throw new Error('Nested compiled isolevel manifest does not list its embedded sprite');
}
if (!nestedArchive[packageAssetPath('probe.png')]) {
  throw new Error('Nested compiled isolevel does not physically contain sprite bytes');
}

const sourceArchive = unzipSync(sourceWorldBytes);
if (!sourceArchive[packageAssetPath('probe.png')] ||
    !sourceArchive[packageAssetPath('world-probe.bin')]) {
  throw new Error('Source isoworld does not physically contain all required asset bytes');
}

console.log(
  'Self-contained package smoke passed: source/compiled isolevels and isoworlds embed their assets, nested compiled levels retain local assets, and missing assets fail packaging.'
);
