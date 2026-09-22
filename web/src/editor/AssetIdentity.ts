export type AssetHash = `sha256:${string}`;

function hex(bytes: Uint8Array): string {
  return Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
}

export async function assetContentHash(bytes: Uint8Array): Promise<AssetHash> {
  const digest = await globalThis.crypto.subtle.digest(
    'SHA-256',
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)
  );
  return `sha256:${hex(new Uint8Array(digest))}`;
}

export async function buildAssetHashIndex(
  assets: Iterable<readonly [string, Uint8Array]>
): Promise<Map<AssetHash, string>> {
  const index = new Map<AssetHash, string>();
  for (const [id, bytes] of assets) {
    const hash = await assetContentHash(bytes);
    if (!index.has(hash)) index.set(hash, id);
  }
  return index;
}

export async function findDuplicateAssetId(
  bytes: Uint8Array,
  existing: Iterable<readonly [string, Uint8Array]>
): Promise<string | undefined> {
  const wanted = await assetContentHash(bytes);
  const index = await buildAssetHashIndex(existing);
  return index.get(wanted);
}
