import { strFromU8, unzipSync } from '../../../vendor/fflate/index';
import type { LevelDocument, LoadedWorldPackage, PackageManifest, WorldDocument } from './documents';
import { validateLevelDocument, validateLoadedWorldPackage, validateManifest, validateWorldDocument } from './validation';

const MAX_ARCHIVE_BYTES = 64 * 1024 * 1024;
const MAX_EXPANDED_BYTES = 128 * 1024 * 1024;
const MAX_FILE_BYTES = 32 * 1024 * 1024;
const MAX_FILE_COUNT = 2048;
const MAX_JSON_BYTES = 8 * 1024 * 1024;
const MAX_PATH_BYTES = 512;

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
    if (!name.endsWith('/')) {
      safePath(name);
      if (size > MAX_FILE_BYTES) throw new Error(`Package file is too large: ${name}`);
      expanded += size;
      if (expanded > MAX_EXPANDED_BYTES) throw new Error('Expanded package is too large');
    }
    offset = end + extraLength + commentLength;
  }
}

class Archive {
  constructor(private readonly files: Record<string, Uint8Array>) {}
  json<T>(path: string): T {
    safePath(path);
    const bytes = this.files[path];
    if (!bytes) throw new Error(`Package entry not found: ${path}`);
    if (bytes.byteLength > MAX_JSON_BYTES) throw new Error(`JSON entry too large: ${path}`);
    try { return JSON.parse(strFromU8(bytes)) as T; }
    catch (error) { throw new Error(`Invalid JSON in ${path}: ${String(error)}`); }
  }
}

export class PackageReader {
  async loadWorld(source: string | Blob | Uint8Array): Promise<LoadedWorldPackage> {
    let bytes: Uint8Array;
    if (typeof source === 'string') {
      const response = await fetch(source, { cache: 'no-store' });
      if (!response.ok) throw new Error(`Unable to load world package: ${response.status}`);
      bytes = new Uint8Array(await response.arrayBuffer());
    } else if (source instanceof Blob) bytes = new Uint8Array(await source.arrayBuffer());
    else bytes = source;
    return this.loadWorldBytes(bytes);
  }

  loadWorldBytes(bytes: Uint8Array): LoadedWorldPackage {
    inspectZip(bytes);
    const archive = new Archive(unzipSync(bytes));
    const manifest = validateManifest(archive.json<PackageManifest>('manifest.json'), 'isoworld');
    const world = validateWorldDocument(archive.json<WorldDocument>(manifest.entry));
    const levels: LevelDocument[] = world.levels.map(reference => {
      const level = validateLevelDocument(archive.json<LevelDocument>(reference.path));
      if (level.id !== reference.id) throw new Error(`Level id mismatch in ${reference.path}`);
      return level;
    });
    return validateLoadedWorldPackage({ manifest, world, levels });
  }
}
