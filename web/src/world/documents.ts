export type Vec3Tuple = [number, number, number];

export type PackageManifest = {
  format: 'isoworld' | 'isolevel';
  representation?: 'source' | 'compiled';
  schemaVersion: number;
  id: string;
  name?: string;
  entry: string;
  minimumEngineVersion?: string;
  createdWith?: { application: string; version: string };
  assets?: Array<{ id: string; path: string; hash?: string }>;
};

export type MaterialDefinition = { id: string; baseColour: Vec3Tuple };
export type RoomSide = 'north' | 'south' | 'east' | 'west';

export type GroundRectangle = {
  id: string;
  type: 'rectangle';
  centre: Vec3Tuple;
  size: [number, number];
  walkable?: boolean;
  material: string;
  alternateMaterial?: string;
};

export type RoomDefinition = {
  id: string;
  centre: Vec3Tuple;
  width: number;
  depth: number;
  floorZ: number;
  wallHeight: number;
  wallThickness: number;
  wallMaterial: string;
};

export type RoomPortalDefinition = {
  roomId: string;
  side: RoomSide;
  offset?: number;
  width: number;
};

export type RoomConnectionDefinition = {
  id: string;
  a: RoomPortalDefinition;
  b: RoomPortalDefinition;
  openPassage?: boolean;
};

export type PrimitiveType =
  | 'cube' | 'sphere' | 'cone' | 'pyramid' | 'dodecahedron' | 'icosahedron';

export type PrimitiveGeometryDefinition = {
  id: string;
  type: PrimitiveType;
  position: Vec3Tuple;
  size: number;
  height?: number;
  material: string;
  solid?: boolean;
};

export type FloorHoleDefinition = {
  id: string;
  type: 'rectangle';
  minimum: [number, number];
  maximum: [number, number];
};

export type StaircaseDefinition = {
  id: string;
  centreX: number;
  startY: number;
  endY: number;
  startZ: number;
  endZ: number;
  width: number;
};

export type PointLightDefinition = {
  id: string;
  type: 'point';
  position: Vec3Tuple;
  enabled?: boolean;
};

export type SpriteAnimationDefinition = {
  resource: string;
  frameCount?: number;
  columns?: number;
  rows?: number;
  fps?: number;
  worldWidth?: number;
  worldHeight?: number;
  loop?: boolean;
};

export type DirectionalSprites = {
  front?: SpriteAnimationDefinition;
  back?: SpriteAnimationDefinition;
  left?: SpriteAnimationDefinition;
  right?: SpriteAnimationDefinition;
};

export type TransformComponentDefinition = { position: Vec3Tuple; forward?: Vec3Tuple };

export type BoxColliderDefinition = {
  type: 'box';
  minimum: Vec3Tuple;
  maximum: Vec3Tuple;
  solid?: boolean;
  collisionTags?: string[];
  mustCollideWith?: string[];
};

export type CharacterEntityDefinition = {
  id: string;
  components: {
    transform: TransformComponentDefinition;
    collider?: BoxColliderDefinition;
    character: {
      npc?: boolean;
      controllable?: boolean;
      movementSpeedMultiplier?: number;
      crouchedHeight?: number;
      sprites?: {
        still?: DirectionalSprites;
        moving?: DirectionalSprites;
        actions?: Record<string, DirectionalSprites>;
      };
    };
  };
};

export type DynamicBodyEntityDefinition = {
  id: string;
  components: {
    transform: TransformComponentDefinition;
    collider: BoxColliderDefinition;
    dynamicBody: {
      surfaceTextureMode?: 'stretch' | 'tile-local' | 'tile-world';
      textureWorldUnitsPerTile?: number;
    };
  };
};

export type EntityDefinition = CharacterEntityDefinition | DynamicBodyEntityDefinition;

export type HazardFaceDefinition =
  | 'any' | 'left' | 'right' | 'back' | 'front' | 'bottom' | 'top';

export type WorldBehaviourDefinition =
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
      blockOnSafeContact?: boolean;
      lethalFace?: HazardFaceDefinition;
      contactTolerance?: number;
    }
  | {
      id: string;
      type: 'rotation';
      entity: string;
      angularSpeed: number;
      directionMultiplier?: number;
    }
  | {
      id: string;
      type: 'hazard';
      entity: string;
      face?: HazardFaceDefinition;
      tolerance?: number;
      action?: 'respawn';
    };

export type LevelDocument = {
  schemaVersion: number;
  id: string;
  name?: string;
  coordinateSystem: { units: 'metres'; upAxis: 'z'; handedness: 'right' };
  viewOrigin: Vec3Tuple;
  ground: GroundRectangle[];
  geometry: PrimitiveGeometryDefinition[];
  rooms?: RoomDefinition[];
  roomConnections?: RoomConnectionDefinition[];
  floorHoles?: FloorHoleDefinition[];
  staircases?: StaircaseDefinition[];
  entities: EntityDefinition[];
  lights: PointLightDefinition[];
  spawns: unknown[];
  connectors: unknown[];
  localMaterials: Record<string, MaterialDefinition>;
  settings: {
    boundsFocus: Vec3Tuple;
    floorDarkMaterial: string;
    floorLightMaterial: string;
    wallMaterial: string;
  };
};

export type WorldConnectorDefinition = {
  id: string;
  type: string;
  fromLevel: string;
  toLevel: string;
  fromPosition: Vec3Tuple;
  toPosition: Vec3Tuple;
  forwardTraversal?: Vec3Tuple[];
  reverseTraversal?: Vec3Tuple[];
  bidirectional?: boolean;
};

export type WorldDocument = {
  schemaVersion: number;
  id: string;
  name?: string;
  settings: {
    defaultLevel: string;
    lowerLevelPreviewDepth?: number;
    lowerPreviewResolutionScale?: number;
    engine?: {
      baseMovementSpeed?: number;
      selection?: {
        mode?: 'multiple' | 'single';
        tint?: Vec3Tuple;
        strength?: number;
      };
    };
  };
  levels: Array<{ id: string; path: string }>;
  assets: Record<string, unknown>;
  materials: Record<string, MaterialDefinition>;
  prefabs: Record<string, unknown>;
  connectors?: WorldConnectorDefinition[];
  behaviours?: WorldBehaviourDefinition[];
  metadata?: Record<string, unknown>;
};

export type LoadedWorldPackage = {
  manifest: PackageManifest;
  world: WorldDocument;
  levels: LevelDocument[];
};
