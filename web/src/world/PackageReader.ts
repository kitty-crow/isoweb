import { strFromU8, unzipSync } from '../../../vendor/fflate/index';
import type {
  LevelDocument, LoadedWorldPackage, PackageManifest, WorldDocument
} from './documents';
import type {
  CompiledLevelDocument, CompiledWorldDocument,
  LoadedCompiledWorldPackage, LoadedRuntimeWorldPackage
} from './WorldCompiler';
import {
  collectCompiledLevelResourceIds, collectLevelResourceIds,
  mergeEmbeddedAssets, readAssetsFromArchive, requireEmbeddedResources,
  type EmbeddedPackageAssetMap
} from './PackageAssets';
import {
  validateCompiledLevelDocument, validateCompiledWorldDocument, validateCompiledWorldGraph
} from './compiledValidation';
import {
  validateLevelDocument, validateLoadedWorldPackage, validateManifest, validateWorldDocument
} from './validation';
import { migrateLevelSource, migrateWorldSource } from './migrations';

const MAX_ARCHIVE_BYTES = 64 * 1024 * 1024;
const MAX_EXPANDED_BYTES = 128 * 1024 * 1024;
const MAX_FILE_BYTES = 32 * 1024 * 1024;
const MAX_FILE_COUNT = 2048;
const MAX_JSON_BYTES = 8 * 1024 * 1024;
const MAX_PATH_BYTES = 512;

export type LoadedLevelPackage = {
  manifest: PackageManifest;
  level: LevelDocument | CompiledLevelDocument;
  assets: EmbeddedPackageAssetMap;
};

function safePath(path: string): void {
  if (!path || path.includes('\\') || path.includes('\0') || path.startsWith('/') || /^[A-Za-z]:/.test(path) ||
      path.split('/').some(part => part === '..' || part === '')) throw new Error(`Unsafe package path: ${path}`);
}

function inspectZip(bytes: Uint8Array): void {
  if (bytes.byteLength > MAX_ARCHIVE_BYTES || bytes.byteLength < 22) throw new Error('Package ZIP size is invalid');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const minimum = Math.max(0, bytes.byteLength - 65_557);
  let eocd = -1;
  for (let offset = bytes.byteLength - 22; offset >= minimum; --offset) {
    if (view.getUint32(offset, true) === 0x06054b50) { eocd = offset; break; }
  }
  if (eocd < 0) throw new Error('ZIP end record is missing');
  const count = view.getUint16(eocd + 10, true);
  const centralSize = view.getUint32(eocd + 12, true);
  const centralOffset = view.getUint32(eocd + 16, true);
  if (count === 0xffff || centralSize === 0xffffffff || centralOffset === 0xffffffff) throw new Error('ZIP64 is unsupported');
  if (count > MAX_FILE_COUNT || centralOffset + centralSize > eocd) throw new Error('ZIP central directory is invalid');
  const decoder = new TextDecoder();
  const names = new Set<string>();
  let offset = centralOffset, expanded = 0;
  for (let index = 0; index < count; ++index) {
    if (offset + 46 > bytes.byteLength || view.getUint32(offset, true) !== 0x02014b50) throw new Error('Malformed ZIP entry');
    const size = view.getUint32(offset + 24, true);
    const nameLength = view.getUint16(offset + 28, true);
    const extraLength = view.getUint16(offset + 30, true);
    const commentLength = view.getUint16(offset + 32, true);
    if (size === 0xffffffff || nameLength === 0 || nameLength > MAX_PATH_BYTES) throw new Error('Unsupported ZIP entry');
    const start = offset + 46, end = start + nameLength;
    if (end > bytes.byteLength) throw new Error('ZIP filename is out of bounds');
    const name = decoder.decode(bytes.subarray(start, end));
    const canonicalName = name.endsWith('/') ? name.slice(0, -1) : name;
    if (canonicalName) safePath(canonicalName);
    if (names.has(name)) throw new Error(`Duplicate package path: ${name}`);
    names.add(name);
    if (!name.endsWith('/')) {
      if (size > MAX_FILE_BYTES) throw new Error(`Package file is too large: ${name}`);
      expanded += size;
      if (expanded > MAX_EXPANDED_BYTES) throw new Error('Expanded package is too large');
    }
    offset = end + extraLength + commentLength;
  }
}

class Archive {
  constructor(private readonly files: Record<string, Uint8Array>) {}

  bytes(path: string): Uint8Array {
    safePath(path);
    const bytes = this.files[path];
    if (!bytes) throw new Error(`Package entry not found: ${path}`);
    return bytes;
  }

  json<T>(path: string): T {
    const bytes = this.bytes(path);
    if (bytes.byteLength > MAX_JSON_BYTES) throw new Error(`JSON entry too large: ${path}`);
    try { return JSON.parse(strFromU8(bytes)) as T; }
    catch (error) { throw new Error(`Invalid JSON in ${path}: ${String(error)}`); }
  }
}

export class PackageReader {
  private async bytes(source: string | Blob | Uint8Array, label: string): Promise<Uint8Array> {
    if (typeof source === 'string') {
      const response = await fetch(source, { cache: 'no-store' });
      if (!response.ok) throw new Error(`Unable to load ${label} package: ${response.status}`);
      return new Uint8Array(await response.arrayBuffer());
    }
    if (source instanceof Blob) return new Uint8Array(await source.arrayBuffer());
    return source;
  }

  async loadLevel(source: string | Blob | Uint8Array): Promise<LevelDocument | CompiledLevelDocument> {
    return (await this.loadLevelPackage(source)).level;
  }

  async loadLevelPackage(source: string | Blob | Uint8Array): Promise<LoadedLevelPackage> {
    return this.loadLevelPackageBytes(await this.bytes(source, 'level'));
  }

  loadLevelBytes(bytes: Uint8Array): LevelDocument | CompiledLevelDocument {
    return this.loadLevelPackageBytes(bytes).level;
  }

  loadLevelPackageBytes(bytes: Uint8Array): LoadedLevelPackage {
    inspectZip(bytes);
    const archive = new Archive(unzipSync(bytes));
    const manifest = validateManifest(archive.json<PackageManifest>('manifest.json'), 'isolevel');
    const assets = readAssetsFromArchive(manifest, path => archive.bytes(path));

    if (manifest.representation === 'compiled') {
      const level = validateCompiledLevelDocument(
        archive.json<CompiledLevelDocument>(manifest.entry)
      );
      if (level.id !== manifest.id) throw new Error('Manifest and compiled level ids disagree');
      requireEmbeddedResources(
        collectCompiledLevelResourceIds(level),
        assets,
        `Compiled level package ${level.id}`
      );
      return { manifest, level, assets };
    }

    const level = validateLevelDocument(migrateLevelSource(archive.json<unknown>(manifest.entry)));
    if (level.id !== manifest.id) throw new Error('Manifest and level ids disagree');
    requireEmbeddedResources(
      collectLevelResourceIds(level),
      assets,
      `Level package ${level.id}`
    );
    return { manifest, level, assets };
  }

  async loadWorld(source: string | Blob | Uint8Array): Promise<LoadedRuntimeWorldPackage> {
    return this.loadWorldBytes(await this.bytes(source, 'world'));
  }

  loadWorldBytes(bytes: Uint8Array): LoadedRuntimeWorldPackage {
    inspectZip(bytes);
    const archive = new Archive(unzipSync(bytes));
    const manifest = validateManifest(archive.json<PackageManifest>('manifest.json'), 'isoworld');
    const assets = readAssetsFromArchive(manifest, path => archive.bytes(path));

    if (manifest.representation === 'compiled') {
      const world = validateCompiledWorldDocument(
        archive.json<CompiledWorldDocument>(manifest.entry)
      );
      requireEmbeddedResources(
        world.assets,
        assets,
        `Compiled world package ${world.id}`
      );
      const levels: CompiledLevelDocument[] = [];
      for (const reference of world.levels) {
        const loaded = this.loadLevelPackageBytes(archive.bytes(reference.path));
        if (!('compiledFormatVersion' in loaded.level)) {
          throw new Error(`Compiled world references source isolevel ${reference.path}`);
        }
        if (loaded.level.id !== reference.id) {
          throw new Error(`Compiled level id mismatch in ${reference.path}`);
        }
        levels.push(loaded.level);
        mergeEmbeddedAssets(assets, loaded.assets, `compiled world ${world.id}`);
      }
      validateCompiledWorldGraph(world, levels);
      if (world.id !== manifest.id) throw new Error('Manifest and compiled world ids disagree');
      return {
        manifest: manifest as LoadedCompiledWorldPackage['manifest'],
        world,
        levels,
        assets
      };
    }

    const world = validateWorldDocument(migrateWorldSource(archive.json<unknown>(manifest.entry)));
    const levels: LevelDocument[] = world.levels.map(reference => {
      const level = validateLevelDocument(migrateLevelSource(archive.json<unknown>(reference.path)));
      if (level.id !== reference.id) throw new Error(`Level id mismatch in ${reference.path}`);
      return level;
    });
    return validateLoadedWorldPackage({
      manifest: manifest as LoadedWorldPackage['manifest'],
      world,
      levels,
      assets
    });
  }
}
