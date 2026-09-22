import type {
  AssetSourceDefinition, DirectionalSprites, LevelDocument, PackageAssetManifestEntry,
  PackageManifest
} from './documents';
import type { CompiledLevelDocument } from './WorldCompiler';

export type PackageAssetInput = {
  bytes: Uint8Array;
  mediaType?: string;
};

export type PackageAssetInputMap =
  | Map<string, PackageAssetInput>
  | Record<string, PackageAssetInput>;

export type EmbeddedPackageAsset = {
  id: string;
  path: string;
  mediaType: string;
  bytes: Uint8Array;
};

export type EmbeddedPackageAssetMap = Map<string, EmbeddedPackageAsset>;

const extensionMediaTypes: Record<string, string> = {
  webp: 'image/webp',
  png: 'image/png',
  jpg: 'image/jpeg',
  jpeg: 'image/jpeg',
  gif: 'image/gif',
  avif: 'image/avif',
  glb: 'model/gltf-binary',
  gltf: 'model/gltf+json',
  bin: 'application/octet-stream',
  json: 'application/json'
};

function nonEmpty(value: string): boolean {
  return value.length > 0;
}

export function validateAssetId(id: string): void {
  if (!nonEmpty(id) || id.includes('\0') || id.length > 512) {
    throw new Error(`Invalid package asset id: ${JSON.stringify(id)}`);
  }
}

export function packageAssetPath(id: string): string {
  validateAssetId(id);
  return `assets/by-id/${encodeURIComponent(id)}`;
}

export function inferAssetMediaType(id: string, explicit?: string): string {
  if (explicit?.trim()) return explicit.trim().toLowerCase();
  const clean = id.split(/[?#]/, 1)[0] ?? id;
  const extension = clean.includes('.') ? clean.split('.').pop()!.toLowerCase() : '';
  return extensionMediaTypes[extension] ?? 'application/octet-stream';
}

export function normaliseAssetInputs(
  input: PackageAssetInputMap | undefined
): Map<string, PackageAssetInput> {
  if (!input) return new Map();
  const entries = input instanceof Map ? input.entries() : Object.entries(input);
  const result = new Map<string, PackageAssetInput>();
  for (const [id, asset] of entries) {
    validateAssetId(id);
    if (!(asset?.bytes instanceof Uint8Array)) {
      throw new Error(`Package asset ${id} does not provide Uint8Array bytes`);
    }
    if (result.has(id)) throw new Error(`Duplicate package asset id: ${id}`);
    result.set(id, {
      bytes: asset.bytes,
      mediaType: inferAssetMediaType(id, asset.mediaType)
    });
  }
  return result;
}

function directionalResources(
  sprites: DirectionalSprites | undefined,
  target: Set<string>
): void {
  if (!sprites) return;
  for (const animation of Object.values(sprites)) {
    if (animation?.resource) target.add(animation.resource);
  }
}

export function collectLevelResourceIds(level: LevelDocument): Set<string> {
  const result = new Set<string>(Object.keys(level.assets ?? {}));
  for (const entity of level.entities) {
    if (!('character' in entity.components)) continue;
    const sprites = entity.components.character.sprites;
    directionalResources(sprites?.still, result);
    directionalResources(sprites?.moving, result);
    for (const directional of Object.values(sprites?.actions ?? {})) {
      directionalResources(directional, result);
    }
  }
  return result;
}

export function collectCompiledLevelResourceIds(level: CompiledLevelDocument): Set<string> {
  const result = new Set<string>();
  for (const entity of level.entities) {
    if (entity.kind !== 'character') continue;
    for (const sprite of entity.sprites) result.add(sprite.animation.resource);
  }
  return result;
}

export function collectWorldDeclaredAssetIds(
  assets: Record<string, AssetSourceDefinition> | undefined
): Set<string> {
  return new Set(Object.keys(assets ?? {}));
}

export function requireAssetInputs(
  required: Iterable<string>,
  input: Map<string, PackageAssetInput>,
  label: string
): void {
  for (const id of required) {
    validateAssetId(id);
    if (!input.has(id)) {
      throw new Error(`${label} is not self-contained: missing embedded asset ${id}`);
    }
  }
}

export function addAssetsToArchive(
  files: Record<string, Uint8Array>,
  input: Map<string, PackageAssetInput>
): PackageAssetManifestEntry[] {
  const manifest: PackageAssetManifestEntry[] = [];
  for (const [id, asset] of input) {
    const path = packageAssetPath(id);
    if (files[path]) throw new Error(`Package asset path collision: ${path}`);
    files[path] = asset.bytes;
    manifest.push({
      id,
      path,
      mediaType: inferAssetMediaType(id, asset.mediaType),
      size: asset.bytes.byteLength
    });
  }
  return manifest.sort((a, b) => a.id.localeCompare(b.id));
}

export function readAssetsFromArchive(
  manifest: PackageManifest,
  read: (path: string) => Uint8Array
): EmbeddedPackageAssetMap {
  const result: EmbeddedPackageAssetMap = new Map();
  for (const entry of manifest.assets ?? []) {
    validateAssetId(entry.id);
    if (result.has(entry.id)) throw new Error(`Duplicate package asset id: ${entry.id}`);
    const bytes = read(entry.path);
    if (entry.size !== undefined && entry.size !== bytes.byteLength) {
      throw new Error(
        `Package asset size mismatch for ${entry.id}: expected ${entry.size}, got ${bytes.byteLength}`
      );
    }
    result.set(entry.id, {
      id: entry.id,
      path: entry.path,
      mediaType: inferAssetMediaType(entry.id, entry.mediaType),
      bytes
    });
  }
  return result;
}

function equalBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.byteLength !== b.byteLength) return false;
  for (let index = 0; index < a.byteLength; ++index) {
    if (a[index] !== b[index]) return false;
  }
  return true;
}

export function mergeEmbeddedAssets(
  target: EmbeddedPackageAssetMap,
  incoming: EmbeddedPackageAssetMap,
  context: string
): void {
  for (const [id, asset] of incoming) {
    const existing = target.get(id);
    if (!existing) {
      target.set(id, asset);
      continue;
    }
    if (
      existing.mediaType !== asset.mediaType ||
      !equalBytes(existing.bytes, asset.bytes)
    ) {
      throw new Error(
        `Asset id ${id} resolves to different bytes while loading ${context}`
      );
    }
  }
}

export function requireEmbeddedResources(
  required: Iterable<string>,
  assets: EmbeddedPackageAssetMap,
  label: string
): void {
  for (const id of required) {
    if (!assets.has(id)) {
      throw new Error(`${label} is not self-contained: referenced asset ${id} is not embedded`);
    }
  }
}
