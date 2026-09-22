import type { IsowebModule } from '../runtime';
import type {
  CharacterEntityDefinition, DirectionalSprites, DynamicBodyEntityDefinition, EntityDefinition,
  LevelDocument, LoadedWorldPackage, MaterialDefinition, PrimitiveType, Vec3Tuple,
  WorldBehaviourDefinition
} from './documents';

type CCallArgType = 'number' | 'string' | 'array';
const ROOM_SIDE = { north: 0, south: 1, east: 2, west: 3 } as const;
const PRIMITIVE_KIND: Record<PrimitiveType, number> = {
  cube: 0, sphere: 1, cone: 2, pyramid: 3, dodecahedron: 4, icosahedron: 5
};
const FACING: Record<keyof DirectionalSprites, number> = { front: 0, back: 1, left: 2, right: 3 };
const TEXTURE_MODE = { stretch: 0, 'tile-local': 1, 'tile-world': 2 } as const;
const HAZARD_FACE = { any: 0, left: 1, right: 2, back: 3, front: 4, bottom: 5, top: 6 } as const;

export class RuntimeWorldBuilder {
  constructor(private readonly module: IsowebModule) {}

  build(packageData: LoadedWorldPackage): Set<string> {
    const { world, levels } = packageData;
    const defaultLevelIndex = levels.findIndex(level => level.id === world.settings.defaultLevel);
    if (defaultLevelIndex < 0) throw new Error(`Default level ${world.settings.defaultLevel} is missing`);
    this.callVoid('isoweb_world_build_begin', ['number','number','number'], [
      defaultLevelIndex,
      world.settings.lowerLevelPreviewDepth ?? 0,
      world.settings.lowerPreviewResolutionScale ?? 0.25
    ]);

    try {
      levels.forEach((level, index) => this.stageLevel(index, level));
      for (const connector of world.connectors ?? []) {
        const index = this.callNumber(
          'isoweb_world_build_add_connector',
          ['string','string','string','string','number','number','number','number','number','number','number'],
          [
            connector.id, connector.type, connector.fromLevel, connector.toLevel,
            ...connector.fromPosition, ...connector.toPosition,
            connector.bidirectional === false ? 0 : 1
          ]
        );
        if (index < 0) throw new Error(`Runtime rejected connector ${connector.id}`);
        for (const sample of connector.forwardTraversal ?? []) {
          this.requireCall(
            'isoweb_world_build_add_connector_forward_sample',
            ['number','number','number','number'], [index, ...sample],
            `connector ${connector.id} forward traversal`
          );
        }
        for (const sample of connector.reverseTraversal ?? []) {
          this.requireCall(
            'isoweb_world_build_add_connector_reverse_sample',
            ['number','number','number','number'], [index, ...sample],
            `connector ${connector.id} reverse traversal`
          );
        }
      }
      if (!this.callNumber('isoweb_world_build_commit', [], [])) {
        throw new Error('Runtime rejected the staged world');
      }
    } catch (error) {
      this.callVoid('isoweb_world_build_cancel', [], []);
      throw error;
    }

    this.applyEngineDefaults(packageData);
    this.callVoid('isoweb_behaviour_clear', [], []);
    const resources = new Set<string>();
    for (const level of levels) {
      for (const entity of level.entities) {
        if (this.isCharacter(entity)) this.applyCharacter(world.id, level.id, entity, resources);
        else this.applyDynamicBody(world.id, level.id, entity);
      }
    }
    for (const behaviour of world.behaviours ?? []) this.applyBehaviour(behaviour);
    return resources;
  }

  private stageLevel(levelIndex: number, level: LevelDocument): void {
    const materials = level.localMaterials;
    const floorDark = this.material(materials, level.settings.floorDarkMaterial, level.id);
    const floorLight = this.material(materials, level.settings.floorLightMaterial, level.id);
    const wall = this.material(materials, level.settings.wallMaterial, level.id);
    const light = level.lights.find(candidate => candidate.enabled !== false && candidate.type === 'point');
    if (!light) throw new Error(`Level ${level.id} has no enabled point light`);

    const nativeIndex = this.callNumber(
      'isoweb_world_build_add_level',
      [
        'string', 'number','number','number', 'number','number','number',
        'number','number','number', 'number','number','number',
        'number','number','number', 'number','number','number'
      ],
      [
        level.id, ...level.viewOrigin, ...light.position,
        ...floorDark.baseColour, ...floorLight.baseColour, ...wall.baseColour,
        ...level.settings.boundsFocus
      ]
    );
    if (nativeIndex !== levelIndex) {
      throw new Error(`Runtime level index mismatch for ${level.id}: expected ${levelIndex}, got ${nativeIndex}`);
    }

    for (const ground of level.ground) {
      this.requireCall(
        'isoweb_world_build_add_ground',
        ['number','number','number','number','number','number','number'],
        [levelIndex, ...ground.centre, ground.size[0], ground.size[1], ground.walkable === false ? 0 : 1],
        `ground ${ground.id}`
      );
    }
    for (const room of level.rooms ?? []) {
      this.requireCall(
        'isoweb_world_build_add_room',
        ['number','string','number','number','number','number','number','number','number'],
        [levelIndex, room.id, room.centre[0], room.centre[1], room.floorZ, room.width, room.depth, room.wallHeight, room.wallThickness],
        `room ${room.id}`
      );
    }
    for (const connection of level.roomConnections ?? []) {
      this.requireCall(
        'isoweb_world_build_add_room_connection',
        ['number','string','string','number','number','number','string','number','number','number','number'],
        [
          levelIndex, connection.id,
          connection.a.roomId, ROOM_SIDE[connection.a.side], connection.a.offset ?? 0, connection.a.width,
          connection.b.roomId, ROOM_SIDE[connection.b.side], connection.b.offset ?? 0, connection.b.width,
          connection.openPassage === false ? 0 : 1
        ],
        `room connection ${connection.id}`
      );
    }
    for (const primitive of level.geometry) {
      const material = this.material(materials, primitive.material, level.id);
      this.requireCall(
        'isoweb_world_build_add_primitive',
        ['number','number','number','number','number','number','number','number','number','number','number'],
        [
          levelIndex, PRIMITIVE_KIND[primitive.type], ...primitive.position,
          primitive.size, primitive.height ?? 0, ...material.baseColour,
          primitive.solid === false ? 0 : 1
        ],
        `primitive ${primitive.id}`
      );
    }
    for (const hole of level.floorHoles ?? []) {
      this.requireCall(
        'isoweb_world_build_add_floor_hole',
        ['number','number','number','number','number'],
        [levelIndex, hole.minimum[0], hole.maximum[0], hole.minimum[1], hole.maximum[1]],
        `floor hole ${hole.id}`
      );
    }
    for (const stair of level.staircases ?? []) {
      this.requireCall(
        'isoweb_world_build_add_staircase',
        ['number','number','number','number','number','number','number'],
        [levelIndex, stair.centreX, stair.startY, stair.endY, stair.startZ, stair.endZ, stair.width],
        `staircase ${stair.id}`
      );
    }
  }

  private material(materials: Record<string, MaterialDefinition>, id: string, levelId: string): MaterialDefinition {
    const material = materials[id];
    if (!material) throw new Error(`Level ${levelId} references missing material ${id}`);
    return material;
  }

  private applyEngineDefaults(packageData: LoadedWorldPackage): void {
    const engine = packageData.world.settings.engine;
    if (!engine) return;
    if (typeof engine.baseMovementSpeed === 'number') this.module._isoweb_set_base_movement_speed(engine.baseMovementSpeed);
    const selection = engine.selection;
    if (!selection) return;
    this.module._isoweb_set_selection_mode(selection.mode === 'single' ? 1 : 0);
    const tint = selection.tint ?? [0.20, 0.48, 1.0];
    this.module._isoweb_set_selection_style(tint[0], tint[1], tint[2], selection.strength ?? 0.45);
  }

  private isCharacter(entity: EntityDefinition): entity is CharacterEntityDefinition {
    return 'character' in entity.components;
  }

  private applyDynamicBody(worldId: string, levelId: string, entity: DynamicBodyEntityDefinition): void {
    const transform = entity.components.transform;
    const collider = entity.components.collider;
    const body = entity.components.dynamicBody;
    const forward = transform.forward ?? [0, 1, 0];
    this.requireCall(
      'isoweb_behaviour_add_entity',
      [
        'string','string','string','string',
        'number','number','number','number','number',
        'number','number','number','number','number','number',
        'number','number','number'
      ],
      [
        entity.id, worldId, 'default', levelId,
        ...transform.position, forward[0], forward[1],
        ...collider.minimum, ...collider.maximum,
        collider.solid === false ? 0 : 1,
        TEXTURE_MODE[body.surfaceTextureMode ?? 'tile-local'],
        body.textureWorldUnitsPerTile ?? 1
      ],
      `dynamic entity ${entity.id}`
    );
    for (const tag of collider.collisionTags ?? []) {
      this.requireCall(
        'isoweb_behaviour_add_entity_collision_tag',
        ['string','string'], [entity.id, tag],
        `dynamic entity ${entity.id} collision tag`
      );
    }
    for (const selector of collider.mustCollideWith ?? []) {
      this.requireCall(
        'isoweb_behaviour_add_entity_collision_selector',
        ['string','string'], [entity.id, selector],
        `dynamic entity ${entity.id} collision selector`
      );
    }
  }

  private applyBehaviour(behaviour: WorldBehaviourDefinition): void {
    switch (behaviour.type) {
      case 'oscillating-gate':
        this.requireCall(
          'isoweb_behaviour_add_gate',
          ['string','string','number','number','number','number','number','number','number','number','number'],
          [
            behaviour.leftEntity, behaviour.rightEntity, ...behaviour.base,
            behaviour.halfSpan, behaviour.gap, behaviour.sweep, behaviour.angularSpeed,
            behaviour.halfThickness, behaviour.height
          ],
          `behaviour ${behaviour.id}`
        );
        return;
      case 'vertical-cycle':
        this.requireCall(
          'isoweb_behaviour_add_vertical_cycle',
          ['string','number','number','number','number','number','number','number','number','number'],
          [
            behaviour.entity, ...behaviour.base, behaviour.upZ, behaviour.downZ, behaviour.period,
            behaviour.blockOnSafeContact === true ? 1 : 0,
            HAZARD_FACE[behaviour.lethalFace ?? 'bottom'],
            behaviour.contactTolerance ?? 0.028
          ],
          `behaviour ${behaviour.id}`
        );
        return;
      case 'rotation':
        this.requireCall(
          'isoweb_behaviour_add_rotation',
          ['string','number','number'],
          [behaviour.entity, behaviour.angularSpeed, behaviour.directionMultiplier ?? 1],
          `behaviour ${behaviour.id}`
        );
        return;
      case 'hazard':
        this.requireCall(
          'isoweb_behaviour_add_hazard',
          ['string','number','number'],
          [behaviour.entity, HAZARD_FACE[behaviour.face ?? 'any'], behaviour.tolerance ?? 0.028],
          `behaviour ${behaviour.id}`
        );
        return;
    }
  }

  private applyCharacter(
    worldId: string, levelId: string, entity: CharacterEntityDefinition, resources: Set<string>
  ): void {
    const transform = entity.components.transform;
    if (!this.callNumber(
      'isoweb_create_character',
      ['string','string','string','string','number','number','number'],
      [entity.id, worldId, 'default', levelId, ...transform.position]
    )) throw new Error(`Runtime rejected Character ${entity.id}`);

    const forward = transform.forward ?? [0, 1, 0];
    this.requireCall(
      'isoweb_set_character_forward', ['string','number','number'],
      [entity.id, forward[0], forward[1]], `Character ${entity.id} forward`
    );

    const collider = entity.components.collider ?? {
      type: 'box' as const, minimum: [-0.25,-0.15,0] as Vec3Tuple,
      maximum: [0.25,0.15,1.70] as Vec3Tuple, solid: true
    };
    this.requireCall(
      'isoweb_set_character_hitbox',
      ['string','number','number','number','number','number','number'],
      [entity.id, ...collider.minimum, ...collider.maximum], `Character ${entity.id} hitbox`
    );

    const character = entity.components.character;
    this.requireCall(
      'isoweb_set_character_flags', ['string','number','number','number'],
      [entity.id, collider.solid === false ? 0 : 1, character.npc === true ? 1 : 0, character.controllable === false ? 0 : 1],
      `Character ${entity.id} flags`
    );
    this.requireCall(
      'isoweb_set_character_speed', ['string','number'],
      [entity.id, character.movementSpeedMultiplier ?? 1], `Character ${entity.id} speed`
    );
    this.requireCall(
      'isoweb_set_character_crouched_height', ['string','number'],
      [entity.id, character.crouchedHeight ?? 0], `Character ${entity.id} crouched height`
    );
    this.requireCall(
      'isoweb_clear_character_collision_filters', ['string'], [entity.id],
      `Character ${entity.id} collision filters`
    );
    for (const tag of collider.collisionTags ?? []) {
      this.requireCall('isoweb_add_character_collision_tag', ['string','string'], [entity.id, tag], `Character ${entity.id} collision tag`);
    }
    for (const selector of collider.mustCollideWith ?? []) {
      this.requireCall('isoweb_add_character_must_collide_with', ['string','string'], [entity.id, selector], `Character ${entity.id} collision selector`);
    }

    this.applyDirectionalSprites(entity.id, 0, '', character.sprites?.still, resources);
    this.applyDirectionalSprites(entity.id, 1, '', character.sprites?.moving, resources);
    for (const [action, sprites] of Object.entries(character.sprites?.actions ?? {})) {
      this.applyDirectionalSprites(entity.id, 2, action, sprites, resources);
    }
  }

  private applyDirectionalSprites(
    id: string, state: number, action: string, sprites: DirectionalSprites | undefined, resources: Set<string>
  ): void {
    if (!sprites) return;
    for (const facing of Object.keys(FACING) as Array<keyof DirectionalSprites>) {
      const animation = sprites[facing];
      if (!animation?.resource) continue;
      resources.add(animation.resource);
      this.requireCall(
        'isoweb_set_character_sprite',
        ['string','number','string','number','string','number','number','number','number','number','number','number'],
        [
          id, state, action, FACING[facing], animation.resource,
          animation.frameCount ?? 1, animation.columns ?? animation.frameCount ?? 1,
          animation.rows ?? 1, animation.fps ?? 6, animation.worldWidth ?? 0,
          animation.worldHeight ?? 0, animation.loop === false ? 0 : 1
        ],
        `Character ${id} sprite ${facing}`
      );
    }
  }

  private callNumber(ident: string, argTypes: CCallArgType[], args: unknown[]): number {
    return Number(this.module.ccall(ident, 'number', argTypes, args));
  }
  private callVoid(ident: string, argTypes: CCallArgType[], args: unknown[]): void {
    this.module.ccall(ident, null, argTypes, args);
  }
  private requireCall(ident: string, argTypes: CCallArgType[], args: unknown[], label: string): void {
    if (!this.callNumber(ident, argTypes, args)) throw new Error(`Runtime rejected ${label}`);
  }
}
