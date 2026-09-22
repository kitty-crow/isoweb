import { strFromU8, unzipSync } from '../vendor/fflate/index';
import { PackageReader } from '../web/src/world/PackageReader';
import { WorldCompiler } from '../web/src/world/WorldCompiler';
import type { PackageManifest } from '../web/src/world/documents';

const bytes = async (path: string) =>
  new Uint8Array(await Bun.file(path).arrayBuffer());

const reader = new PackageReader();
const compiler = new WorldCompiler();

const compiledBytes = await bytes('web/assets/demo.isoworld');
const compiled = reader.loadWorldBytes(compiledBytes);
if (compiled.manifest.representation !== 'compiled') {
  throw new Error('Canonical demo.isoworld is not compiled');
}
if (!('compiledFormatVersion' in compiled.world)) {
  throw new Error('Canonical demo.isoworld did not load a compiled world projection');
}

const archive = unzipSync(compiledBytes);
const manifest = JSON.parse(strFromU8(archive['manifest.json'])) as PackageManifest;
if (manifest.representation !== 'compiled' || manifest.entry !== 'runtime/world.json') {
  throw new Error('Compiled world manifest is invalid');
}

for (const reference of compiled.world.levels) {
  if (!reference.path.endsWith('.isolevel')) {
    throw new Error(\`Compiled world level \${reference.id} is not an isolevel package\`);
  }
  const nestedBytes = archive[reference.path];
  if (!nestedBytes) throw new Error(\`Compiled world is missing nested \${reference.path}\`);
  const nested = unzipSync(nestedBytes);
  const nestedManifest = JSON.parse(strFromU8(nested['manifest.json'])) as PackageManifest;
  if (
    nestedManifest.format !== 'isolevel' ||
    nestedManifest.representation !== 'compiled' ||
    nestedManifest.entry !== 'runtime/level.json'
  ) {
    throw new Error(\`Nested compiled isolevel \${reference.id} has an invalid manifest\`);
  }
}

const source = reader.loadWorldBytes(await bytes('web/assets/demo-source.isoworld'));
if (source.manifest.representation !== 'source') {
  throw new Error('demo-source.isoworld is not a source/document package');
}
if ('compiledFormatVersion' in source.world) {
  throw new Error('Source package unexpectedly contains compiled world data');
}

const freshlyCompiled = compiler.compilePackage(source);
if (JSON.stringify(freshlyCompiled.world) !== JSON.stringify(compiled.world)) {
  throw new Error('Deployment compiled world differs from the shared WorldCompiler output');
}
if (JSON.stringify(freshlyCompiled.levels) !== JSON.stringify(compiled.levels)) {
  throw new Error('Deployment compiled levels differ from the shared WorldCompiler output');
}

for (const level of compiled.levels) {
  const standalone = reader.loadLevelBytes(await bytes(\`web/assets/demo-\${level.id}.isolevel\`));
  if (!('compiledFormatVersion' in standalone) || JSON.stringify(standalone) !== JSON.stringify(level)) {
    throw new Error(\`Standalone compiled isolevel \${level.id} differs from the isoworld member\`);
  }

  const sourceLevel = reader.loadLevelBytes(await bytes(\`web/assets/demo-\${level.id}-source.isolevel\`));
  if ('compiledFormatVersion' in sourceLevel || sourceLevel.id !== level.id) {
    throw new Error(\`Source isolevel compatibility artifact \${level.id} is invalid\`);
  }
}

console.log(
  'Compiled package smoke passed: source world -> shared compiler -> compiled isolevels -> nested compiled isoworld, with source packages still loadable.'
);
