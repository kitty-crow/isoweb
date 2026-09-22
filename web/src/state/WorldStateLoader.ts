import type { IsowebModule } from '../runtime';
import type { EmbeddedPackageAsset } from '../world/PackageAssets';
import { PackageReader } from '../world/PackageReader';
import { RuntimeWorldBuilder } from '../world/RuntimeWorldBuilder';

type CCallArgType = 'number' | 'string' | 'array';

export class WorldStateLoader {
  private readonly packages = new PackageReader();
  private readonly builder: RuntimeWorldBuilder;

  constructor(private readonly module: IsowebModule) {
    this.builder = new RuntimeWorldBuilder(module);
  }

  async load(url = new URL('assets/demo.isoworld', document.baseURI).toString()): Promise<void> {
    const packageData = await this.packages.loadWorld(url);
    const resources = this.builder.build(packageData);
    await Promise.all(Array.from(resources, async resource => {
      const asset = packageData.assets.get(resource);
      if (!asset) {
        throw new Error(
          `World package is not self-contained: runtime resource ${resource} has no embedded asset`
        );
      }
      await this.loadEmbeddedAtlas(asset);
    }));
  }

  private callNumber(ident: string, argTypes: CCallArgType[], args: unknown[]): number {
    return Number(this.module.ccall(ident, 'number', argTypes, args));
  }

  private async loadEmbeddedAtlas(asset: EmbeddedPackageAsset): Promise<void> {
    if (!asset.mediaType.startsWith('image/')) {
      throw new Error(
        `Character artwork ${asset.id} has unsupported embedded media type ${asset.mediaType}`
      );
    }

    const bitmap = await createImageBitmap(
      new Blob([asset.bytes], { type: asset.mediaType })
    );
    const canvas = document.createElement('canvas');
    canvas.width = bitmap.width;
    canvas.height = bitmap.height;
    const context = canvas.getContext('2d', { willReadFrequently: true });
    if (!context) throw new Error(`Unable to decode embedded character artwork ${asset.id}`);
    context.drawImage(bitmap, 0, 0);
    bitmap.close();

    const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
    const bytes = new Uint8Array(pixels.buffer, pixels.byteOffset, pixels.byteLength);
    const pointer = this.module._malloc(bytes.byteLength);
    if (!pointer) {
      throw new Error(`Unable to allocate WASM memory for embedded character artwork ${asset.id}`);
    }

    try {
      this.module.HEAPU8.set(bytes, pointer);
      if (!this.callNumber(
        'isoweb_register_sprite_atlas',
        ['string','number','number','number','number'],
        [asset.id, canvas.width, canvas.height, pointer, bytes.byteLength]
      )) {
        throw new Error(`WASM rejected embedded character artwork ${asset.id}`);
      }
    } finally {
      this.module._free(pointer);
    }
  }
}
