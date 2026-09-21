import type { IsowebModule } from '../runtime';
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
    await Promise.all(Array.from(resources, resource => this.loadWebPAtlas(resource)));
  }

  private callNumber(ident: string, argTypes: CCallArgType[], args: unknown[]): number {
    return Number(this.module.ccall(ident, 'number', argTypes, args));
  }

  private async loadWebPAtlas(resource: string): Promise<void> {
    const response = await fetch(new URL(resource, document.baseURI));
    if (!response.ok) throw new Error(`Unable to load character artwork ${resource}: ${response.status}`);
    const bitmap = await createImageBitmap(await response.blob());
    const canvas = document.createElement('canvas');
    canvas.width = bitmap.width;
    canvas.height = bitmap.height;
    const context = canvas.getContext('2d', { willReadFrequently: true });
    if (!context) throw new Error(`Unable to decode character artwork ${resource}`);
    context.drawImage(bitmap, 0, 0);
    bitmap.close();
    const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
    const bytes = new Uint8Array(pixels.buffer, pixels.byteOffset, pixels.byteLength);
    const pointer = this.module._malloc(bytes.byteLength);
    if (!pointer) throw new Error(`Unable to allocate WASM memory for character artwork ${resource}`);
    try {
      this.module.HEAPU8.set(bytes, pointer);
      if (!this.callNumber(
        'isoweb_register_sprite_atlas', ['string','number','number','number','number'],
        [resource, canvas.width, canvas.height, pointer, bytes.byteLength]
      )) throw new Error(`WASM rejected character artwork ${resource}`);
    } finally {
      this.module._free(pointer);
    }
  }
}
