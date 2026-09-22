import type { LevelDocument, PackageManifest, WorldDocument } from '../world/documents';
import {
  type EmbeddedPackageAssetMap, type PackageAssetInputMap
} from '../world/PackageAssets';
import { PackageReader } from '../world/PackageReader';
import { PackageWriter } from '../world/PackageWriter';

export type EditableLevelProject = {
  kind: 'level';
  manifest: PackageManifest & { representation?: 'source' };
  document: LevelDocument;
  assets: EmbeddedPackageAssetMap;
};

export type EditableWorldProject = {
  kind: 'world';
  manifest: PackageManifest & { representation?: 'source' };
  document: WorldDocument;
  levels: LevelDocument[];
  assets: EmbeddedPackageAssetMap;
};

export type EditableSourceProject = EditableLevelProject | EditableWorldProject;
export type ProjectSource = string | Blob | Uint8Array;

function sourceAssetInputs(assets: EmbeddedPackageAssetMap): PackageAssetInputMap {
  const result = new Map<string, { bytes: Uint8Array; mediaType?: string }>();
  for (const [id, asset] of assets) {
    result.set(id, { bytes: asset.bytes, mediaType: asset.mediaType });
  }
  return result;
}

export class SourceProjectIO {
  constructor(
    private readonly reader = new PackageReader(),
    private readonly writer = new PackageWriter()
  ) {}

  async openLevel(source: ProjectSource): Promise<EditableLevelProject> {
    const loaded = await this.reader.loadLevelPackage(source);
    if (loaded.manifest.representation === 'compiled' ||
        'compiledFormatVersion' in loaded.level) {
      throw new Error('Compiled .isolevel packages are runtime products and cannot be edited as source');
    }
    return {
      kind: 'level',
      manifest: loaded.manifest as EditableLevelProject['manifest'],
      document: loaded.level,
      assets: loaded.assets
    };
  }

  async openWorld(source: ProjectSource): Promise<EditableWorldProject> {
    const loaded = await this.reader.loadWorld(source);
    if (loaded.manifest.representation === 'compiled' ||
        'compiledFormatVersion' in loaded.world) {
      throw new Error('Compiled .isoworld packages are runtime products and cannot be edited as source');
    }
    return {
      kind: 'world',
      manifest: loaded.manifest as EditableWorldProject['manifest'],
      document: loaded.world,
      levels: loaded.levels,
      assets: loaded.assets
    };
  }

  saveLevelBytes(project: EditableLevelProject): Uint8Array {
    return this.writer.writeLevelBytes(project.document, sourceAssetInputs(project.assets));
  }

  saveWorldBytes(project: EditableWorldProject): Uint8Array {
    return this.writer.writeWorldBytes(
      project.document,
      project.levels,
      sourceAssetInputs(project.assets)
    );
  }

  previewWorldBytes(project: EditableSourceProject): Uint8Array {
    if (project.kind === 'world') return this.saveWorldBytes(project);

    const level = project.document;
    const world: WorldDocument = {
      schemaVersion: level.schemaVersion,
      id: `preview-${level.id}`,
      name: `${level.name ?? level.id} Preview`,
      settings: { defaultLevel: level.id },
      levels: [{ id: level.id, path: `levels/${level.id}.json` }],
      assets: {},
      materials: {},
      prefabs: {},
      connectors: [],
      behaviours: [],
      metadata: { editorPreview: true }
    };

    return this.writer.writeWorldBytes(
      world,
      [level],
      sourceAssetInputs(project.assets)
    );
  }

  saveLevel(project: EditableLevelProject): Blob {
    return new Blob([this.saveLevelBytes(project)], { type: 'application/octet-stream' });
  }

  saveWorld(project: EditableWorldProject): Blob {
    return new Blob([this.saveWorldBytes(project)], { type: 'application/octet-stream' });
  }
}
