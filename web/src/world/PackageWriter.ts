import { strToU8, zipSync } from '../../../vendor/fflate/index';
import type { LevelDocument, PackageManifest, WorldDocument } from './documents';
import { CURRENT_SCHEMA_VERSION, validateLevelDocument, validateWorldDocument } from './validation';

const json = (value: unknown): Uint8Array => strToU8(JSON.stringify(value, null, 2) + '\n');

export class PackageWriter {
  writeLevelBytes(level: LevelDocument): Uint8Array {
    validateLevelDocument(level);
    const manifest: PackageManifest = {
      format: 'isolevel', schemaVersion: CURRENT_SCHEMA_VERSION, id: level.id, name: level.name,
      entry: 'level.json', createdWith: { application: 'isoweb-level-builder', version: '0.1.0' }
    };
    return zipSync({ 'manifest.json': json(manifest), 'level.json': json(level) }, { level: 6 });
  }

  writeWorldBytes(world: WorldDocument, levels: LevelDocument[]): Uint8Array {
    validateWorldDocument(world);
    const byId = new Map(levels.map(level => [level.id, validateLevelDocument(level)]));
    const files: Record<string, Uint8Array> = {};
    for (const reference of world.levels) {
      const level = byId.get(reference.id);
      if (!level) throw new Error(`Missing level ${reference.id}`);
      files[reference.path] = json(level);
    }
    const manifest: PackageManifest = {
      format: 'isoworld', schemaVersion: CURRENT_SCHEMA_VERSION, id: world.id, name: world.name,
      entry: 'world.json', minimumEngineVersion: '0.1.0',
      createdWith: { application: 'isoweb-world-editor', version: '0.1.0' }, assets: []
    };
    files['manifest.json'] = json(manifest);
    files['world.json'] = json(world);
    return zipSync(files, { level: 6 });
  }

  writeLevel(level: LevelDocument): Blob {
    return new Blob([this.writeLevelBytes(level)], { type: 'application/octet-stream' });
  }
  writeWorld(world: WorldDocument, levels: LevelDocument[]): Blob {
    return new Blob([this.writeWorldBytes(world, levels)], { type: 'application/octet-stream' });
  }
}
