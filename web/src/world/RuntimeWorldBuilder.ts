import type { IsowebModule } from '../runtime';
import {
  WorldCompiler,
  type CompiledBehaviour,
  type CompiledCharacterEntity,
  type CompiledDynamicEntity,
  type CompiledLevelDocument,
  type CompiledWorldDocument,
  type LoadedCompiledWorldPackage,
  type LoadedRuntimeWorldPackage
} from './WorldCompiler';

type CCallArgType = 'number' | 'string' | 'array';

export class RuntimeWorldBuilder {
  private readonly compiler = new WorldCompiler();

  constructor(private readonly module: IsowebModule) {}

  build(packageData: LoadedRuntimeWorldPackage): Set<string> {
    const compiled: LoadedCompiledWorldPackage =
      packageData.manifest.representation === 'compiled'
        ? packageData
        : this.compiler.compilePackage(packageData);

    return this.buildCompiled(compiled);
  }

  private buildCompiled(packageData: LoadedCompiledWorldPackage): Set<string> {
    const { world, levels } = packageData;
    this.callVoid('isoweb_world_build_begin', ['number','number','number'], [
      world.defaultLevelIndex,
      world.lowerLevelPreviewDepth,
      world.lowerPreviewResolutionScale
    ]);

    try {
      levels.forEach((level, index) => this.stageLevel(index, level));
      for (const connector of world.connectors) {
        const index = this.callNumber(
          'isoweb_world_build_add_connector',
          ['string','string','string','string','number','number','number','number','number','number','number'],
          [
            connector.id, connector.type, connector.fromLevel, connector.toLevel,
            ...connector.fromPosition, ...connector.toPosition, connector.bidirectional
          ]
        );
        if (index < 0) throw new Error(`Runtime rejected connector ${connector.id}`);
        for (const sample of connector.forwardTraversal) {
          this.requireCall(
            'isoweb_world_build_add_connector_forward_sample',
            ['number','number','number','number'], [index, ...sample],
            `connector ${connector.id} forward traversal`
          );
        }
        for (const sample of connector.reverseTraversal) {
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

    this.applyEngineDefaults(world);
    this.callVoid('isoweb_behaviour_clear', [], []);

    const resources = new Set<string>();
    for (const level of levels) {
      for (const entity of level.entities) {
        if (entity.kind === 'character') {
          this.applyCharacter(world.id, level.id, entity, resources);
        } else {
          this.applyDynamicBody(world.id, level.id, entity);
        }
      }
    }
    for (const behaviour of world.behaviours) this.applyBehaviour(behaviour);
    return resources;
  }

  private stageLevel(levelIndex: number, level: CompiledLevelDocument): void {
    const nativeIndex = this.callNumber(
      'isoweb_world_build_add_level',
      [
        'string', 'number','number','number', 'number','number','number',
        'number','number','number', 'number','number','number',
        'number','number','number', 'number','number','number'
      ],
      [
        level.id, ...level.viewOrigin, ...level.lightPosition,
        ...level.floorDark, ...level.floorLight, ...level.wallColour, ...level.boundsFocus
      ]
    );
    if (nativeIndex !== levelIndex) {
      throw new Error(`Runtime level index mismatch for ${level.id}: expected ${levelIndex}, got ${nativeIndex}`);
    }

    for (const ground of level.ground) {
      this.requireCall(
        'isoweb_world_build_add_ground',
        ['number','number','number','number','number','number','number'],
        [levelIndex, ...ground.centre, ground.width, ground.depth, ground.walkable],
        `compiled ground in ${level.id}`
      );
    }
    for (const room of level.rooms) {
      this.requireCall(
        'isoweb_world_build_add_room',
        ['number','string','number','number','number','number','number','number','number'],
        [
          levelIndex, room.id, room.centreX, room.centreY, room.floorZ,
          room.width, room.depth, room.wallHeight, room.wallThickness
        ],
        `compiled room ${room.id}`
      );
    }
    for (const connection of level.roomConnections) {
      this.requireCall(
        'isoweb_world_build_add_room_connection',
        ['number','string','string','number','number','number','string','number','number','number','number'],
        [
          levelIndex, connection.id,
          connection.a.roomId, connection.a.side, connection.a.offset, connection.a.width,
          connection.b.roomId, connection.b.side, connection.b.offset, connection.b.width,
          connection.openPassage
        ],
        `compiled room connection ${connection.id}`
      );
    }
    for (const primitive of level.primitives) {
      this.requireCall(
        'isoweb_world_build_add_primitive',
        ['number','number','number','number','number','number','number','number','number','number','number'],
        [
          levelIndex, primitive.kind, ...primitive.position, primitive.size, primitive.height,
          ...primitive.colour, primitive.solid
        ],
        `compiled primitive in ${level.id}`
      );
    }
    for (const hole of level.floorHoles) {
      this.requireCall(
        'isoweb_world_build_add_floor_hole',
        ['number','number','number','number','number'],
        [levelIndex, hole.minimumX, hole.maximumX, hole.minimumY, hole.maximumY],
        `compiled floor hole in ${level.id}`
      );
    }
    for (const stair of level.staircases) {
      this.requireCall(
        'isoweb_world_build_add_staircase',
        ['number','number','number','number','number','number','number'],
        [
          levelIndex, stair.centreX, stair.startY, stair.endY,
          stair.startZ, stair.endZ, stair.width
        ],
        `compiled staircase in ${level.id}`
      );
    }
  }

  private applyEngineDefaults(world: CompiledWorldDocument): void {
    const engine = world.engine;
    if (typeof engine.baseMovementSpeed === 'number') {
      this.module._isoweb_set_base_movement_speed(engine.baseMovementSpeed);
    }
    if (typeof engine.selectionMode === 'number') {
      this.module._isoweb_set_selection_mode(engine.selectionMode);
    }
    if (engine.selectionTint) {
      this.module._isoweb_set_selection_style(
        engine.selectionTint[0],
        engine.selectionTint[1],
        engine.selectionTint[2],
        engine.selectionStrength ?? 0.45
      );
    }
  }

  private applyDynamicBody(worldId: string, levelId: string, entity: CompiledDynamicEntity): void {
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
        ...entity.position, ...entity.forward,
        ...entity.hitBoxMinimum, ...entity.hitBoxMaximum,
        entity.solid, entity.textureMode, entity.textureWorldUnitsPerTile
      ],
      `compiled dynamic entity ${entity.id}`
    );
    for (const tag of entity.collisionTags) {
      this.requireCall(
        'isoweb_behaviour_add_entity_collision_tag',
        ['string','string'], [entity.id, tag],
        `compiled dynamic entity ${entity.id} collision tag`
      );
    }
    for (const selector of entity.mustCollideWith) {
      this.requireCall(
        'isoweb_behaviour_add_entity_collision_selector',
        ['string','string'], [entity.id, selector],
        `compiled dynamic entity ${entity.id} collision selector`
      );
    }
  }

  private applyBehaviour(behaviour: CompiledBehaviour): void {
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
          `compiled behaviour ${behaviour.id}`
        );
        return;
      case 'vertical-cycle':
        this.requireCall(
          'isoweb_behaviour_add_vertical_cycle',
          ['string','number','number','number','number','number','number','number','number','number'],
          [
            behaviour.entity, ...behaviour.base, behaviour.upZ, behaviour.downZ, behaviour.period,
            behaviour.blockOnSafeContact, behaviour.lethalFace, behaviour.contactTolerance
          ],
          `compiled behaviour ${behaviour.id}`
        );
        return;
      case 'rotation':
        this.requireCall(
          'isoweb_behaviour_add_rotation',
          ['string','number','number'],
          [behaviour.entity, behaviour.angularSpeed, behaviour.directionMultiplier],
          `compiled behaviour ${behaviour.id}`
        );
        return;
      case 'hazard':
        this.requireCall(
          'isoweb_behaviour_add_hazard',
          ['string','number','number'],
          [behaviour.entity, behaviour.face, behaviour.tolerance],
          `compiled behaviour ${behaviour.id}`
        );
        return;
    }
  }

  private applyCharacter(
    worldId: string,
    levelId: string,
    entity: CompiledCharacterEntity,
    resources: Set<string>
  ): void {
    if (!this.callNumber(
      'isoweb_create_character',
      ['string','string','string','string','number','number','number'],
      [entity.id, worldId, 'default', levelId, ...entity.position]
    )) throw new Error(`Runtime rejected compiled Character ${entity.id}`);

    this.requireCall(
      'isoweb_set_character_forward', ['string','number','number'],
      [entity.id, ...entity.forward], `compiled Character ${entity.id} forward`
    );
    this.requireCall(
      'isoweb_set_character_hitbox',
      ['string','number','number','number','number','number','number'],
      [entity.id, ...entity.hitBoxMinimum, ...entity.hitBoxMaximum],
      `compiled Character ${entity.id} hitbox`
    );
    this.requireCall(
      'isoweb_set_character_flags', ['string','number','number','number'],
      [entity.id, entity.solid, entity.npc, entity.controllable],
      `compiled Character ${entity.id} flags`
    );
    this.requireCall(
      'isoweb_set_character_speed', ['string','number'],
      [entity.id, entity.movementSpeedMultiplier], `compiled Character ${entity.id} speed`
    );
    this.requireCall(
      'isoweb_set_character_crouched_height', ['string','number'],
      [entity.id, entity.crouchedHeight], `compiled Character ${entity.id} crouched height`
    );
    this.requireCall(
      'isoweb_clear_character_collision_filters', ['string'], [entity.id],
      `compiled Character ${entity.id} collision filters`
    );
    for (const tag of entity.collisionTags) {
      this.requireCall(
        'isoweb_add_character_collision_tag', ['string','string'], [entity.id, tag],
        `compiled Character ${entity.id} collision tag`
      );
    }
    for (const selector of entity.mustCollideWith) {
      this.requireCall(
        'isoweb_add_character_must_collide_with', ['string','string'], [entity.id, selector],
        `compiled Character ${entity.id} collision selector`
      );
    }

    for (const binding of entity.sprites) {
      const animation = binding.animation;
      resources.add(animation.resource);
      this.requireCall(
        'isoweb_set_character_sprite',
        ['string','number','string','number','string','number','number','number','number','number','number','number'],
        [
          entity.id, binding.state, binding.action, binding.facing, animation.resource,
          animation.frameCount, animation.columns, animation.rows, animation.fps,
          animation.worldWidth, animation.worldHeight, animation.loop ? 1 : 0
        ],
        `compiled Character ${entity.id} sprite`
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
