import type { IsowebModule } from '../runtime';

type Vec3Tuple = [number, number, number];

type SpriteAnimationDefinition = {
  resource: string;
  frameCount?: number;
  columns?: number;
  rows?: number;
  fps?: number;
  worldWidth?: number;
  worldHeight?: number;
  loop?: boolean;
};

type DirectionalSprites = {
  front?: SpriteAnimationDefinition;
  back?: SpriteAnimationDefinition;
  left?: SpriteAnimationDefinition;
  right?: SpriteAnimationDefinition;
};

type CharacterDefinition = {
  id: string;
  location: {
    world: string;
    timeline: string;
    level: string;
    position: Vec3Tuple;
  };
  forward?: [number, number];
  hitBox?: {
    minimum: Vec3Tuple;
    maximum: Vec3Tuple;
  };
  solid?: boolean;
  npc?: boolean;
  controllable?: boolean;
  movementSpeedMultiplier?: number;
  collisionTags?: string[];
  mustCollideWith?: string[];
  sprites?: {
    still?: DirectionalSprites;
    moving?: DirectionalSprites;
    actions?: Record<string, DirectionalSprites>;
  };
};

type WorldState = {
  engine?: {
    baseMovementSpeed?: number;
    selection?: {
      mode?: 'multiple' | 'single';
      tint?: Vec3Tuple;
      strength?: number;
    };
  };
  characters?: CharacterDefinition[];
};

const FACING: Record<keyof DirectionalSprites, number> = {
  front: 0,
  back: 1,
  left: 2,
  right: 3
};

class CStringPool {
  private readonly pointers: number[] = [];
  private readonly encoder = new TextEncoder();

  constructor(private readonly module: IsowebModule) {}

  add(value: string | undefined): number {
    const bytes = this.encoder.encode(value ?? '');
    const pointer = this.module._malloc(bytes.length + 1);
    this.module.HEAPU8.set(bytes, pointer);
    this.module.HEAPU8[pointer + bytes.length] = 0;
    this.pointers.push(pointer);
    return pointer;
  }

  dispose(): void {
    for (const pointer of this.pointers) this.module._free(pointer);
    this.pointers.length = 0;
  }
}

export class WorldStateLoader {
  constructor(private readonly module: IsowebModule) {}

  async load(url = new URL('assets/default-world.json', document.baseURI).toString()): Promise<void> {
    const response = await fetch(url, { cache: 'no-store' });
    if (!response.ok) throw new Error(`Unable to load world state: ${response.status}`);
    const state = await response.json() as WorldState;
    this.applyEngineDefaults(state);
    this.module._isoweb_clear_entities();

    const resources = new Set<string>();
    for (const character of state.characters ?? []) {
      this.applyCharacter(character, resources);
    }

    await Promise.all(Array.from(resources, resource => this.loadWebPAtlas(resource)));
    this.module._isoweb_render();
  }

  private applyEngineDefaults(state: WorldState): void {
    const engine = state.engine;
    if (!engine) return;
    if (typeof engine.baseMovementSpeed === 'number') {
      this.module._isoweb_set_base_movement_speed(engine.baseMovementSpeed);
    }
    const selection = engine.selection;
    if (!selection) return;
    this.module._isoweb_set_selection_mode(selection.mode === 'single' ? 1 : 0);
    const tint = selection.tint ?? [0.20, 0.48, 1.0];
    this.module._isoweb_set_selection_style(
      tint[0], tint[1], tint[2], selection.strength ?? 0.45
    );
  }

  private applyCharacter(character: CharacterDefinition, resources: Set<string>): void {
    const pool = new CStringPool(this.module);
    try {
      const id = pool.add(character.id);
      const world = pool.add(character.location.world);
      const timeline = pool.add(character.location.timeline);
      const level = pool.add(character.location.level);
      const [x, y, z] = character.location.position;
      if (!this.module._isoweb_create_character(id, world, timeline, level, x, y, z)) return;

      const forward = character.forward ?? [0, 1];
      this.module._isoweb_set_character_forward(id, forward[0], forward[1]);

      const hitBox = character.hitBox ?? {
        minimum: [-0.25, -0.15, 0],
        maximum: [0.25, 0.15, 1.70]
      };
      this.module._isoweb_set_character_hitbox(
        id,
        hitBox.minimum[0], hitBox.minimum[1], hitBox.minimum[2],
        hitBox.maximum[0], hitBox.maximum[1], hitBox.maximum[2]
      );
      this.module._isoweb_set_character_flags(
        id,
        character.solid === false ? 0 : 1,
        character.npc === true ? 1 : 0,
        character.controllable === false ? 0 : 1
      );
      this.module._isoweb_set_character_speed(id, character.movementSpeedMultiplier ?? 1);

      this.module._isoweb_clear_character_collision_filters(id);
      for (const tag of character.collisionTags ?? []) {
        this.module._isoweb_add_character_collision_tag(id, pool.add(tag));
      }
      for (const selector of character.mustCollideWith ?? []) {
        this.module._isoweb_add_character_must_collide_with(id, pool.add(selector));
      }

      this.applyDirectionalSprites(id, 0, '', character.sprites?.still, resources, pool);
      this.applyDirectionalSprites(id, 1, '', character.sprites?.moving, resources, pool);
      for (const [action, directions] of Object.entries(character.sprites?.actions ?? {})) {
        this.applyDirectionalSprites(id, 2, action, directions, resources, pool);
      }
    } finally {
      pool.dispose();
    }
  }

  private applyDirectionalSprites(
    id: number,
    state: number,
    action: string,
    sprites: DirectionalSprites | undefined,
    resources: Set<string>,
    pool: CStringPool
  ): void {
    if (!sprites) return;
    const actionPointer = pool.add(action);
    for (const facing of Object.keys(FACING) as Array<keyof DirectionalSprites>) {
      const animation = sprites[facing];
      if (!animation?.resource) continue;
      resources.add(animation.resource);
      this.module._isoweb_set_character_sprite(
        id,
        state,
        actionPointer,
        FACING[facing],
        pool.add(animation.resource),
        animation.frameCount ?? 1,
        animation.columns ?? animation.frameCount ?? 1,
        animation.rows ?? 1,
        animation.fps ?? 6,
        animation.worldWidth ?? 0,
        animation.worldHeight ?? 0,
        animation.loop === false ? 0 : 1
      );
    }
  }

  private async loadWebPAtlas(resource: string): Promise<void> {
    const response = await fetch(new URL(resource, document.baseURI));
    if (!response.ok) throw new Error(`Unable to load character artwork ${resource}: ${response.status}`);
    const blob = await response.blob();
    const bitmap = await createImageBitmap(blob);
    const canvas = document.createElement('canvas');
    canvas.width = bitmap.width;
    canvas.height = bitmap.height;
    const context = canvas.getContext('2d', { willReadFrequently: true });
    if (!context) throw new Error(`Unable to decode character artwork ${resource}`);
    context.drawImage(bitmap, 0, 0);
    bitmap.close();
    const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;

    const pool = new CStringPool(this.module);
    const dataPointer = this.module._malloc(pixels.byteLength);
    try {
      this.module.HEAPU8.set(pixels, dataPointer);
      const resourcePointer = pool.add(resource);
      if (!this.module._isoweb_register_sprite_atlas(
        resourcePointer,
        canvas.width,
        canvas.height,
        dataPointer,
        pixels.byteLength
      )) {
        throw new Error(`WASM rejected character artwork ${resource}`);
      }
    } finally {
      this.module._free(dataPointer);
      pool.dispose();
    }
  }
}
