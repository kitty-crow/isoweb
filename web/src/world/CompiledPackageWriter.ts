import { strToU8, zipSync } from '../../../vendor/fflate/index';
import type { PackageManifest } from './documents';
import type { CompiledLevelDocument, CompiledWorldDocument } from './WorldCompiler';
import {
  addAssetsToArchive, collectCompiledLevelResourceIds, normaliseAssetInputs,
  requireAssetInputs, type PackageAssetInputMap
} from './PackageAssets';
import {
  validateCompiledLevelDocument, validateCompiledWorldDocument, validateCompiledWorldGraph
} from './compiledValidation';
import { CURRENT_SCHEMA_VERSION } from './validation';

const json = (value: unknown): Uint8Array => strToU8(JSON.stringify(value, null, 2) + '\n');

export class CompiledPackageWriter {
  writeLevelBytes(
    level: CompiledLevelDocument,
    assets?: PackageAssetInputMap
  ): Uint8Array {
    validateCompiledLevelDocument(level);
    const inputs = normaliseAssetInputs(assets);
    requireAssetInputs(
      collectCompiledLevelResourceIds(level),
      inputs,
      `Compiled level ${level.id}`
    );

    const files: Record<string, Uint8Array> = {
      'runtime/level.json': json(level)
    };
    const manifestAssets = addAssetsToArchive(files, inputs);
    const manifest: PackageManifest = {
      format: 'isolevel',
      representation: 'compiled',
      schemaVersion: CURRENT_SCHEMA_VERSION,
      id: level.id,
      name: level.name,
      entry: 'runtime/level.json',
      minimumEngineVersion: '0.1.0',
      createdWith: { application: 'isoweb-world-compiler', version: '0.1.0' },
      assets: manifestAssets
    };
    files['manifest.json'] = json(manifest);
    return zipSync(files, { level: 6 });
  }

  writeWorldBytes(
    world: CompiledWorldDocument,
    levels: CompiledLevelDocument[],
    levelPackages?: Map<string, Uint8Array>,
    worldAssets?: PackageAssetInputMap
  ): Uint8Array {
    validateCompiledWorldDocument(world);
    levels.forEach(validateCompiledLevelDocument);
    validateCompiledWorldGraph(world, levels);

    const byId = new Map(levels.map(level => [level.id, level]));
    const files: Record<string, Uint8Array> = {};
    for (const reference of world.levels) {
      const level = byId.get(reference.id);
      if (!level) throw new Error(`Missing compiled level ${reference.id}`);
      const packaged = levelPackages?.get(reference.id);
      if (!packaged) {
        if (collectCompiledLevelResourceIds(level).size > 0) {
          throw new Error(
            `Compiled world ${world.id} requires a self-contained isolevel package for ${level.id}`
          );
        }
        files[reference.path] = this.writeLevelBytes(level);
      } else {
        files[reference.path] = packaged;
      }
    }

    const inputs = normaliseAssetInputs(worldAssets);
    const manifestAssets = addAssetsToArchive(files, inputs);
    const manifest: PackageManifest = {
      format: 'isoworld',
      representation: 'compiled',
      schemaVersion: CURRENT_SCHEMA_VERSION,
      id: world.id,
      name: world.name,
      entry: 'runtime/world.json',
      minimumEngineVersion: '0.1.0',
      createdWith: { application: 'isoweb-world-compiler', version: '0.1.0' },
      assets: manifestAssets
    };
    files['manifest.json'] = json(manifest);
    files['runtime/world.json'] = json(world);
    return zipSync(files, { level: 6 });
  }
}
