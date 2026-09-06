import type { IsowebModule } from '../runtime';

type Vec3Tuple = [number, number, number];
type CCallArgType = 'number' | 'string' | 'array';

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

  private callNumber(ident: string, argTypes: CCallArgType[], args: unknown[]): number {
    return Number(this.module.ccall(ident, 'number', argTypes, args));
  }

  private callVoid(ident: string, argTypes: CCallArgType[], args: unknown[]): void {
    this.module.ccall(ident, null, argTypes, args);
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
    const [x, y, z] = character.location.position;
    if (!this.callNumber(
      'isoweb_create_character',
      ['string', 'string', 'string', 'string', 'number', 'number', 'number'],
      [
        character.id,
        character.location.world,
        character.location.timeline,
        character.location.level,
        x,
        y,
        z
      ]
    )) return;

    const forward = character.forward ?? [0, 1];
    this.callNumber(
      'isoweb_set_character_forward',
      ['string', 'number', 'number'],
      [character.id, forward[0], forward[1]]
    );

    const hitBox = character.hitBox ?? {
      minimum: [-0.25, -0.15, 0] as Vec3Tuple,
      maximum: [0.25, 0.15, 1.70] as Vec3Tuple
    };
    this.callNumber(
      'isoweb_set_character_hitbox',
      ['string', 'number', 'number', 'number', 'number', 'number', 'number'],
      [
        character.id,
        hitBox.minimum[0], hitBox.minimum[1], hitBox.minimum[2],
        hitBox.maximum[0], hitBox.maximum[1], hitBox.maximum[2]
      ]
    );
    this.callNumber(
      'isoweb_set_character_flags',
      ['string', 'number', 'number', 'number'],
      [
        character.id,
        character.solid === false ? 0 : 1,
        character.npc === true ? 1 : 0,
        character.controllable === false ? 0 : 1
      ]
    );
    this.callNumber(
      'isoweb_set_character_speed',
      ['string', 'number'],
      [character.id, character.movementSpeedMultiplier ?? 1]
    );

    this.callNumber(
      'isoweb_clear_character_collision_filters',
      ['string'],
      [character.id]
    );
    for (const tag of character.collisionTags ?? []) {
      this.callNumber(
        'isoweb_add_character_collision_tag',
        ['string', 'string'],
        [character.id, tag]
      );
    }
    for (const selector of character.mustCollideWith ?? []) {
      this.callNumber(
        'isoweb_add_character_must_collide_with',
        ['string', 'string'],
        [character.id, selector]
      );
    }

    this.applyDirectionalSprites(character.id, 0, '', character.sprites?.still, resources);
    this.applyDirectionalSprites(character.id, 1, '', character.sprites?.moving, resources);
    for (const [action, directions] of Object.entries(character.sprites?.actions ?? {})) {
      this.applyDirectionalSprites(character.id, 2, action, directions, resources);
    }
  }

  private applyDirectionalSprites(
    id: string,
    state: number,
    action: string,
    sprites: DirectionalSprites | undefined,
    resources: Set<string>
  ): void {
    if (!sprites) return;
    for (const facing of Object.keys(FACING) as Array<keyof DirectionalSprites>) {
      const animation = sprites[facing];
      if (!animation?.resource) continue;
      resources.add(animation.resource);
      this.callNumber(
        'isoweb_set_character_sprite',
        [
          'string', 'number', 'string', 'number', 'string',
          'number', 'number', 'number', 'number', 'number', 'number', 'number'
        ],
        [
          id,
          state,
          action,
          FACING[facing],
          animation.resource,
          animation.frameCount ?? 1,
          animation.columns ?? animation.frameCount ?? 1,
          animation.rows ?? 1,
          animation.fps ?? 6,
          animation.worldWidth ?? 0,
          animation.worldHeight ?? 0,
          animation.loop === false ? 0 : 1
        ]
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
    const bytes = new Uint8Array(pixels.buffer, pixels.byteOffset, pixels.byteLength);

    if (!this.callNumber(
      'isoweb_register_sprite_atlas',
      ['string', 'number', 'number', 'array', 'number'],
      [resource, canvas.width, canvas.height, bytes, bytes.byteLength]
    )) {
      throw new Error(`WASM rejected character artwork ${resource}`);
    }
  }
}
