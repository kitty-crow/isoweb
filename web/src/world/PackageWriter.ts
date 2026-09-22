import { strToU8, zipSync } from '../../../vendor/fflate/index';
import type { LevelDocument, PackageManifest, WorldDocument } from './documents';
import {
  addAssetsToArchive, collectLevelResourceIds, collectWorldDeclaredAssetIds,
  normaliseAssetInputs, requireAssetInputs, type PackageAssetInputMap
} from './PackageAssets';
import { CURRENT_SCHEMA_VERSION, validateLevelDocument, validateWorldDocument } from './validation';

const json = (value: unknown): Uint8Array => strToU8(JSON.stringify(value, null, 2) + '\n');

export class PackageWriter {
  writeLevelBytes(level: LevelDocument, assets?: PackageAssetInputMap): Uint8Array {
    validateLevelDocument(level);
    const inputs = normaliseAssetInputs(assets);
    requireAssetInputs(
      collectLevelResourceIds(level),
      inputs,
      `Level ${level.id}`
    );

    const files: Record<string, Uint8Array> = {
      'level.json': json(level)
    };
    const manifestAssets = addAssetsToArchive(files, inputs);
    const manifest: PackageManifest = {
      format: 'isolevel',
      representation: 'source',
      schemaVersion: CURRENT_SCHEMA_VERSION,
      id: level.id,
      name: level.name,
      entry: 'level.json',
      createdWith: { application: 'isoweb-level-builder', version: '0.1.0' },
      assets: manifestAssets
    };
    files['manifest.json'] = json(manifest);
    return zipSync(files, { level: 6 });
  }

  writeWorldBytes(
    world: WorldDocument,
    levels: LevelDocument[],
    assets?: PackageAssetInputMap
  ): Uint8Array {
    validateWorldDocument(world);
    const byId = new Map(levels.map(level => [level.id, validateLevelDocument(level)]));
    const inputs = normaliseAssetInputs(assets);
    const required = collectWorldDeclaredAssetIds(world.assets);
    for (const level of levels) {
      for (const id of collectLevelResourceIds(level)) required.add(id);
    }
    requireAssetInputs(required, inputs, `World ${world.id}`);

    const files: Record<string, Uint8Array> = {};
    for (const reference of world.levels) {
      const level = byId.get(reference.id);
      if (!level) throw new Error(`Missing level ${reference.id}`);
      files[reference.path] = json(level);
    }
    const manifestAssets = addAssetsToArchive(files, inputs);
    const manifest: PackageManifest = {
      format: 'isoworld',
      representation: 'source',
      schemaVersion: CURRENT_SCHEMA_VERSION,
      id: world.id,
      name: world.name,
      entry: 'world.json',
      minimumEngineVersion: '0.1.0',
      createdWith: { application: 'isoweb-world-editor', version: '0.1.0' },
      assets: manifestAssets
    };
    files['manifest.json'] = json(manifest);
    files['world.json'] = json(world);
    return zipSync(files, { level: 6 });
  }

  writeLevel(level: LevelDocument, assets?: PackageAssetInputMap): Blob {
    return new Blob([this.writeLevelBytes(level, assets)], { type: 'application/octet-stream' });
  }

  writeWorld(
    world: WorldDocument,
    levels: LevelDocument[],
    assets?: PackageAssetInputMap
  ): Blob {
    return new Blob(
      [this.writeWorldBytes(world, levels, assets)],
      { type: 'application/octet-stream' }
    );
  }
}
