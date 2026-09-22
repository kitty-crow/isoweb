import type {
  DirectionalSprites, HazardFaceDefinition, LevelDocument, PackageManifest,
  SpriteAnimationDefinition, Vec3Tuple, WorldBehaviourDefinition, WorldDocument
} from './documents';

export const COMPILED_FORMAT_VERSION = 1;

export type CompiledGround = {
  centre: Vec3Tuple;
  width: number;
  depth: number;
  walkable: number;
};

export type CompiledRoom = {
  id: string;
  centreX: number;
  centreY: number;
  floorZ: number;
  width: number;
  depth: number;
  wallHeight: number;
  wallThickness: number;
};

export type CompiledRoomConnection = {
  id: string;
  a: { roomId: string; side: number; offset: number; width: number };
  b: { roomId: string; side: number; offset: number; width: number };
  openPassage: number;
};

export type CompiledPrimitive = {
  kind: number;
  position: Vec3Tuple;
  size: number;
  height: number;
  colour: Vec3Tuple;
  solid: number;
};

export type CompiledFloorHole = {
  minimumX: number;
  maximumX: number;
  minimumY: number;
  maximumY: number;
};

export type CompiledStaircase = {
  centreX: number;
  startY: number;
  endY: number;
  startZ: number;
  endZ: number;
  width: number;
};

export type CompiledSpriteBinding = {
  state: number;
  action: string;
  facing: number;
  animation: Required<Omit<SpriteAnimationDefinition, 'resource'>> & { resource: string };
};

export type CompiledCharacterEntity = {
  kind: 'character';
  id: string;
  position: Vec3Tuple;
  forward: [number, number];
  hitBoxMinimum: Vec3Tuple;
  hitBoxMaximum: Vec3Tuple;
  solid: number;
  npc: number;
  controllable: number;
  movementSpeedMultiplier: number;
  crouchedHeight: number;
  collisionTags: string[];
  mustCollideWith: string[];
  sprites: CompiledSpriteBinding[];
};

export type CompiledDynamicEntity = {
  kind: 'dynamic';
  id: string;
  position: Vec3Tuple;
  forward: [number, number];
  hitBoxMinimum: Vec3Tuple;
  hitBoxMaximum: Vec3Tuple;
  solid: number;
  textureMode: number;
  textureWorldUnitsPerTile: number;
  collisionTags: string[];
  mustCollideWith: string[];
};

export type CompiledEntity = CompiledCharacterEntity | CompiledDynamicEntity;

export type CompiledLevelDocument = {
  schemaVersion: number;
  compiledFormatVersion: number;
  id: string;
  name?: string;
  viewOrigin: Vec3Tuple;
  lightPosition: Vec3Tuple;
  floorDark: Vec3Tuple;
  floorLight: Vec3Tuple;
  wallColour: Vec3Tuple;
  boundsFocus: Vec3Tuple;
  ground: CompiledGround[];
  rooms: CompiledRoom[];
  roomConnections: CompiledRoomConnection[];
  primitives: CompiledPrimitive[];
  floorHoles: CompiledFloorHole[];
  staircases: CompiledStaircase[];
  entities: CompiledEntity[];
};

export type CompiledConnector = {
  id: string;
  type: string;
  fromLevel: string;
  toLevel: string;
  fromPosition: Vec3Tuple;
  toPosition: Vec3Tuple;
  forwardTraversal: Vec3Tuple[];
  reverseTraversal: Vec3Tuple[];
  bidirectional: number;
};

export type CompiledBehaviour =
  | {
      id: string;
      type: 'oscillating-gate';
      leftEntity: string;
      rightEntity: string;
      base: Vec3Tuple;
      halfSpan: number;
      gap: number;
      sweep: number;
      angularSpeed: number;
      halfThickness: number;
      height: number;
    }
  | {
      id: string;
      type: 'vertical-cycle';
      entity: string;
      base: Vec3Tuple;
      upZ: number;
      downZ: number;
      period: number;
      blockOnSafeContact: number;
      lethalFace: number;
      contactTolerance: number;
    }
  | {
      id: string;
      type: 'rotation';
      entity: string;
      angularSpeed: number;
      directionMultiplier: number;
    }
  | {
      id: string;
      type: 'hazard';
      entity: string;
      face: number;
      tolerance: number;
      action: 'respawn';
    };

export type CompiledWorldDocument = {
  schemaVersion: number;
  compiledFormatVersion: number;
  id: string;
  name?: string;
  defaultLevelIndex: number;
  lowerLevelPreviewDepth: number;
  lowerPreviewResolutionScale: number;
  engine: {
    baseMovementSpeed?: number;
    selectionMode?: number;
    selectionTint?: Vec3Tuple;
    selectionStrength?: number;
  };
  levels: Array<{ id: string; path: string }>;
  connectors: CompiledConnector[];
  behaviours: CompiledBehaviour[];
};

export type LoadedSourceWorldPackage = {
  manifest: PackageManifest & { representation?: 'source' };
  world: WorldDocument;
  levels: LevelDocument[];
};

export type LoadedCompiledWorldPackage = {
  manifest: PackageManifest & { representation: 'compiled' };
  world: CompiledWorldDocument;
  levels: CompiledLevelDocument[];
};

export type LoadedRuntimeWorldPackage = LoadedSourceWorldPackage | LoadedCompiledWorldPackage;

const ROOM_SIDE = { north: 0, south: 1, east: 2, west: 3 } as const;
const PRIMITIVE_KIND = {
  cube: 0, sphere: 1, cone: 2, pyramid: 3, dodecahedron: 4, icosahedron: 5
} as const;
const FACING = { front: 0, back: 1, left: 2, right: 3 } as const;
const TEXTURE_MODE = { stretch: 0, 'tile-local': 1, 'tile-world': 2 } as const;
const HAZARD_FACE: Record<HazardFaceDefinition, number> = {
  any: 0, left: 1, right: 2, back: 3, front: 4, bottom: 5, top: 6
};

function requiredAnimation(animation: SpriteAnimationDefinition): CompiledSpriteBinding['animation'] {
  return {
    resource: animation.resource,
    frameCount: animation.frameCount ?? 1,
    columns: animation.columns ?? animation.frameCount ?? 1,
    rows: animation.rows ?? 1,
    fps: animation.fps ?? 6,
    worldWidth: animation.worldWidth ?? 0,
    worldHeight: animation.worldHeight ?? 0,
    loop: animation.loop ?? true
  };
}

function compileSprites(
  state: number,
  action: string,
  sprites: DirectionalSprites | undefined,
  target: CompiledSpriteBinding[]
): void {
  if (!sprites) return;
  for (const facing of Object.keys(FACING) as Array<keyof DirectionalSprites>) {
    const animation = sprites[facing];
    if (!animation?.resource) continue;
    target.push({ state, action, facing: FACING[facing], animation: requiredAnimation(animation) });
  }
}

export class WorldCompiler {
  compileLevel(level: LevelDocument): CompiledLevelDocument {
    const floorDark = level.localMaterials[level.settings.floorDarkMaterial];
    const floorLight = level.localMaterials[level.settings.floorLightMaterial];
    const wall = level.localMaterials[level.settings.wallMaterial];
    if (!floorDark || !floorLight || !wall) throw new Error(`Level ${level.id} has unresolved presentation materials`);
    const light = level.lights.find(candidate => candidate.enabled !== false && candidate.type === 'point');
    if (!light) throw new Error(`Level ${level.id} has no enabled point light`);

    const entities: CompiledEntity[] = level.entities.map(entity => {
      const transform = entity.components.transform;
      const forward: [number, number] = [transform.forward?.[0] ?? 0, transform.forward?.[1] ?? 1];
      const collider = entity.components.collider ?? {
        type: 'box' as const,
        minimum: [-0.25, -0.15, 0] as Vec3Tuple,
        maximum: [0.25, 0.15, 1.70] as Vec3Tuple,
        solid: true
      };

      if ('character' in entity.components) {
        const character = entity.components.character;
        const sprites: CompiledSpriteBinding[] = [];
        compileSprites(0, '', character.sprites?.still, sprites);
        compileSprites(1, '', character.sprites?.moving, sprites);
        for (const [action, directional] of Object.entries(character.sprites?.actions ?? {})) {
          compileSprites(2, action, directional, sprites);
        }
        return {
          kind: 'character' as const,
          id: entity.id,
          position: transform.position,
          forward,
          hitBoxMinimum: collider.minimum,
          hitBoxMaximum: collider.maximum,
          solid: collider.solid === false ? 0 : 1,
          npc: character.npc === true ? 1 : 0,
          controllable: character.controllable === false ? 0 : 1,
          movementSpeedMultiplier: character.movementSpeedMultiplier ?? 1,
          crouchedHeight: character.crouchedHeight ?? 0,
          collisionTags: collider.collisionTags ?? [],
          mustCollideWith: collider.mustCollideWith ?? [],
          sprites
        };
      }

      const body = entity.components.dynamicBody;
      return {
        kind: 'dynamic' as const,
        id: entity.id,
        position: transform.position,
        forward,
        hitBoxMinimum: collider.minimum,
        hitBoxMaximum: collider.maximum,
        solid: collider.solid === false ? 0 : 1,
        textureMode: TEXTURE_MODE[body.surfaceTextureMode ?? 'tile-local'],
        textureWorldUnitsPerTile: body.textureWorldUnitsPerTile ?? 1,
        collisionTags: collider.collisionTags ?? [],
        mustCollideWith: collider.mustCollideWith ?? []
      };
    });

    return {
      schemaVersion: level.schemaVersion,
      compiledFormatVersion: COMPILED_FORMAT_VERSION,
      id: level.id,
      name: level.name,
      viewOrigin: level.viewOrigin,
      lightPosition: light.position,
      floorDark: floorDark.baseColour,
      floorLight: floorLight.baseColour,
      wallColour: wall.baseColour,
      boundsFocus: level.settings.boundsFocus,
      ground: level.ground.map(ground => ({
        centre: ground.centre,
        width: ground.size[0],
        depth: ground.size[1],
        walkable: ground.walkable === false ? 0 : 1
      })),
      rooms: (level.rooms ?? []).map(room => ({
        id: room.id,
        centreX: room.centre[0],
        centreY: room.centre[1],
        floorZ: room.floorZ,
        width: room.width,
        depth: room.depth,
        wallHeight: room.wallHeight,
        wallThickness: room.wallThickness
      })),
      roomConnections: (level.roomConnections ?? []).map(connection => ({
        id: connection.id,
        a: {
          roomId: connection.a.roomId,
          side: ROOM_SIDE[connection.a.side],
          offset: connection.a.offset ?? 0,
          width: connection.a.width
        },
        b: {
          roomId: connection.b.roomId,
          side: ROOM_SIDE[connection.b.side],
          offset: connection.b.offset ?? 0,
          width: connection.b.width
        },
        openPassage: connection.openPassage === false ? 0 : 1
      })),
      primitives: level.geometry.map(primitive => {
        const material = level.localMaterials[primitive.material];
        if (!material) throw new Error(`Level ${level.id} references missing material ${primitive.material}`);
        return {
          kind: PRIMITIVE_KIND[primitive.type],
          position: primitive.position,
          size: primitive.size,
          height: primitive.height ?? 0,
          colour: material.baseColour,
          solid: primitive.solid === false ? 0 : 1
        };
      }),
      floorHoles: (level.floorHoles ?? []).map(hole => ({
        minimumX: hole.minimum[0],
        maximumX: hole.maximum[0],
        minimumY: hole.minimum[1],
        maximumY: hole.maximum[1]
      })),
      staircases: (level.staircases ?? []).map(stair => ({
        centreX: stair.centreX,
        startY: stair.startY,
        endY: stair.endY,
        startZ: stair.startZ,
        endZ: stair.endZ,
        width: stair.width
      })),
      entities
    };
  }

  compileWorld(world: WorldDocument, compiledLevelPaths?: Map<string, string>): CompiledWorldDocument {
    const defaultLevelIndex = world.levels.findIndex(level => level.id === world.settings.defaultLevel);
    if (defaultLevelIndex < 0) throw new Error(`Default level ${world.settings.defaultLevel} is missing`);
    const selection = world.settings.engine?.selection;
    return {
      schemaVersion: world.schemaVersion,
      compiledFormatVersion: COMPILED_FORMAT_VERSION,
      id: world.id,
      name: world.name,
      defaultLevelIndex,
      lowerLevelPreviewDepth: world.settings.lowerLevelPreviewDepth ?? 0,
      lowerPreviewResolutionScale: world.settings.lowerPreviewResolutionScale ?? 0.25,
      engine: {
        baseMovementSpeed: world.settings.engine?.baseMovementSpeed,
        selectionMode: selection ? (selection.mode === 'single' ? 1 : 0) : undefined,
        selectionTint: selection?.tint,
        selectionStrength: selection?.strength
      },
      levels: world.levels.map(reference => ({
        id: reference.id,
        path: compiledLevelPaths?.get(reference.id) ?? `levels/${reference.id}.isolevel`
      })),
      connectors: (world.connectors ?? []).map(connector => ({
        id: connector.id,
        type: connector.type,
        fromLevel: connector.fromLevel,
        toLevel: connector.toLevel,
        fromPosition: connector.fromPosition,
        toPosition: connector.toPosition,
        forwardTraversal: connector.forwardTraversal ?? [],
        reverseTraversal: connector.reverseTraversal ?? [],
        bidirectional: connector.bidirectional === false ? 0 : 1
      })),
      behaviours: (world.behaviours ?? []).map(behaviour => this.compileBehaviour(behaviour))
    };
  }

  compilePackage(source: { manifest: PackageManifest; world: WorldDocument; levels: LevelDocument[] }): LoadedCompiledWorldPackage {
    const levelPaths = new Map(source.world.levels.map(reference => [reference.id, `levels/${reference.id}.isolevel`]));
    return {
      manifest: {
        ...source.manifest,
        representation: 'compiled',
        entry: 'runtime/world.json'
      },
      world: this.compileWorld(source.world, levelPaths),
      levels: source.levels.map(level => this.compileLevel(level))
    };
  }

  private compileBehaviour(behaviour: WorldBehaviourDefinition): CompiledBehaviour {
    switch (behaviour.type) {
      case 'oscillating-gate':
        return { ...behaviour };
      case 'vertical-cycle':
        return {
          ...behaviour,
          blockOnSafeContact: behaviour.blockOnSafeContact === true ? 1 : 0,
          lethalFace: HAZARD_FACE[behaviour.lethalFace ?? 'bottom'],
          contactTolerance: behaviour.contactTolerance ?? 0.028
        };
      case 'rotation':
        return { ...behaviour, directionMultiplier: behaviour.directionMultiplier ?? 1 };
      case 'hazard':
        return {
          ...behaviour,
          face: HAZARD_FACE[behaviour.face ?? 'any'],
          tolerance: behaviour.tolerance ?? 0.028,
          action: behaviour.action ?? 'respawn'
        };
    }
  }
}
