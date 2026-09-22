import { strToU8, zipSync } from '../../../vendor/fflate/index';
import type { PackageManifest } from './documents';
import type { CompiledLevelDocument, CompiledWorldDocument } from './WorldCompiler';
import {
  validateCompiledLevelDocument, validateCompiledWorldDocument, validateCompiledWorldGraph
} from './compiledValidation';
import { CURRENT_SCHEMA_VERSION } from './validation';

const json = (value: unknown): Uint8Array => strToU8(JSON.stringify(value, null, 2) + '\n');

export class CompiledPackageWriter {
  writeLevelBytes(level: CompiledLevelDocument): Uint8Array {
    validateCompiledLevelDocument(level);
    const manifest: PackageManifest = {
      format: 'isolevel',
      representation: 'compiled',
      schemaVersion: CURRENT_SCHEMA_VERSION,
      id: level.id,
      name: level.name,
      entry: 'runtime/level.json',
      minimumEngineVersion: '0.1.0',
      createdWith: { application: 'isoweb-world-compiler', version: '0.1.0' }
    };
    return zipSync({
      'manifest.json': json(manifest),
      'runtime/level.json': json(level)
    }, { level: 6 });
  }

  writeWorldBytes(
    world: CompiledWorldDocument,
    levels: CompiledLevelDocument[],
    levelPackages?: Map<string, Uint8Array>
  ): Uint8Array {
    validateCompiledWorldDocument(world);
    levels.forEach(validateCompiledLevelDocument);
    validateCompiledWorldGraph(world, levels);

    const byId = new Map(levels.map(level => [level.id, level]));
    const files: Record<string, Uint8Array> = {};
    for (const reference of world.levels) {
      const level = byId.get(reference.id);
      if (!level) throw new Error(`Missing compiled level ${reference.id}`);
      const packaged = levelPackages?.get(reference.id) ?? this.writeLevelBytes(level);
      files[reference.path] = packaged;
    }

    const manifest: PackageManifest = {
      format: 'isoworld',
      representation: 'compiled',
      schemaVersion: CURRENT_SCHEMA_VERSION,
      id: world.id,
      name: world.name,
      entry: 'runtime/world.json',
      minimumEngineVersion: '0.1.0',
      createdWith: { application: 'isoweb-world-compiler', version: '0.1.0' },
      assets: []
    };
    files['manifest.json'] = json(manifest);
    files['runtime/world.json'] = json(world);
    return zipSync(files, { level: 6 });
  }
}
