import type {
  LevelDocument, LoadedWorldPackage, MaterialDefinition, PackageManifest, Vec3Tuple, WorldDocument
} from './documents';

export const CURRENT_SCHEMA_VERSION = 1;
const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);
const nonEmpty = (value: unknown): value is string => typeof value === 'string' && value.length > 0;
const vec3 = (value: unknown): value is Vec3Tuple =>
  Array.isArray(value) && value.length === 3 && value.every(finite);
const safePackagePath = (value: string): boolean =>
  !!value && !value.includes('\\') && !value.includes('\0') && !value.startsWith('/') &&
  !/^[A-Za-z]:/.test(value) && value.split('/').every(part => part !== '..' && part !== '');

function object(value: unknown, label: string): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as Record<string, unknown>;
}

export function validateManifest(value: unknown, expected: PackageManifest['format']): PackageManifest {
  const manifest = object(value, 'manifest');
  if (manifest.format !== expected) throw new Error(`Expected ${expected} package`);
  if (manifest.schemaVersion !== CURRENT_SCHEMA_VERSION) {
    throw new Error(`Unsupported manifest schema version ${String(manifest.schemaVersion)}`);
  }
  if (!nonEmpty(manifest.id)) throw new Error('Manifest id must be non-empty');
  if (!nonEmpty(manifest.entry) || !safePackagePath(manifest.entry)) throw new Error('Manifest entry is unsafe');
  return manifest as PackageManifest;
}

function materials(value: unknown, label: string): Record<string, MaterialDefinition> {
  const registry = object(value, label);
  for (const [id, raw] of Object.entries(registry)) {
    const material = object(raw, `${label}.${id}`);
    if (material.id !== id || !vec3(material.baseColour)) throw new Error(`Invalid material ${id}`);
  }
  return registry as Record<string, MaterialDefinition>;
}

export function validateLevelDocument(value: unknown): LevelDocument {
  const level = object(value, 'level');
  if (level.schemaVersion !== CURRENT_SCHEMA_VERSION || !nonEmpty(level.id) || !vec3(level.viewOrigin)) {
    throw new Error('Level header is invalid');
  }
  for (const key of ['ground', 'geometry', 'entities', 'lights', 'spawns', 'connectors']) {
    if (!Array.isArray(level[key])) throw new Error(`Level ${level.id} ${key} must be an array`);
  }
  const coordinate = object(level.coordinateSystem, 'coordinateSystem');
  if (coordinate.units !== 'metres' || coordinate.upAxis !== 'z' || coordinate.handedness !== 'right') {
    throw new Error(`Level ${level.id} coordinate system is unsupported`);
  }

  const registry = materials(level.localMaterials, `Level ${level.id} materials`);
  const settings = object(level.settings, `Level ${level.id} settings`);
  if (!vec3(settings.boundsFocus)) throw new Error(`Level ${level.id} boundsFocus is invalid`);
  for (const key of ['floorDarkMaterial', 'floorLightMaterial', 'wallMaterial']) {
    const id = settings[key];
    if (!nonEmpty(id) || !registry[id]) throw new Error(`Level ${level.id} references missing material`);
  }

  const groundIds = new Set<string>();
  for (const raw of level.ground as unknown[]) {
    const ground = object(raw, 'ground');
    if (!nonEmpty(ground.id) || groundIds.has(ground.id) || ground.type !== 'rectangle' || !vec3(ground.centre)) {
      throw new Error(`Invalid ground in level ${level.id}`);
    }
    groundIds.add(ground.id);
    if (!Array.isArray(ground.size) || ground.size.length !== 2 || !ground.size.every(finite) ||
        ground.size.some(size => size <= 0)) throw new Error(`Invalid ground size ${ground.id}`);
    if (!nonEmpty(ground.material) || !registry[ground.material]) throw new Error(`Missing ground material ${ground.id}`);
    if (ground.alternateMaterial !== undefined &&
        (!nonEmpty(ground.alternateMaterial) || !registry[ground.alternateMaterial])) {
      throw new Error(`Missing alternate ground material ${ground.id}`);
    }
  }

  const roomIds = new Set<string>();
  for (const raw of (level.rooms as unknown[] | undefined) ?? []) {
    const room = object(raw, 'room');
    if (!nonEmpty(room.id) || roomIds.has(room.id) || !vec3(room.centre)) throw new Error('Invalid room');
    roomIds.add(room.id);
    for (const key of ['width', 'depth', 'floorZ', 'wallHeight', 'wallThickness']) {
      if (!finite(room[key])) throw new Error(`Room ${room.id} has invalid ${key}`);
    }
    if ((room.width as number) <= 0 || (room.depth as number) <= 0) throw new Error(`Room ${room.id} is empty`);
    if (!nonEmpty(room.wallMaterial) || !registry[room.wallMaterial]) throw new Error(`Room ${room.id} material is missing`);
  }

  const connectionIds = new Set<string>();
  for (const raw of (level.roomConnections as unknown[] | undefined) ?? []) {
    const connection = object(raw, 'room connection');
    if (!nonEmpty(connection.id) || connectionIds.has(connection.id)) throw new Error('Invalid room connection id');
    connectionIds.add(connection.id);
    for (const endpointKey of ['a', 'b']) {
      const endpoint = object(connection[endpointKey], 'room portal');
      if (!nonEmpty(endpoint.roomId) || !roomIds.has(endpoint.roomId) ||
          !['north','south','east','west'].includes(String(endpoint.side)) ||
          !finite(endpoint.width) || endpoint.width <= 0 ||
          (endpoint.offset !== undefined && !finite(endpoint.offset))) {
        throw new Error(`Room connection ${connection.id} has invalid portal`);
      }
    }
  }

  const geometryIds = new Set<string>();
  const primitives = new Set(['cube','sphere','cone','pyramid','dodecahedron','icosahedron']);
  for (const raw of level.geometry as unknown[]) {
    const geometry = object(raw, 'geometry');
    if (!nonEmpty(geometry.id) || geometryIds.has(geometry.id) || !primitives.has(String(geometry.type)) ||
        !vec3(geometry.position) || !finite(geometry.size) || geometry.size <= 0 ||
        !nonEmpty(geometry.material) || !registry[geometry.material]) {
      throw new Error(`Invalid primitive in level ${level.id}`);
    }
    geometryIds.add(geometry.id);
  }

  for (const raw of (level.floorHoles as unknown[] | undefined) ?? []) {
    const hole = object(raw, 'floor hole');
    if (hole.type !== 'rectangle' || !Array.isArray(hole.minimum) || !Array.isArray(hole.maximum) ||
        hole.minimum.length !== 2 || hole.maximum.length !== 2 ||
        !hole.minimum.every(finite) || !hole.maximum.every(finite) ||
        hole.minimum[0] >= hole.maximum[0] || hole.minimum[1] >= hole.maximum[1]) {
      throw new Error('Invalid floor hole');
    }
  }

  for (const raw of (level.staircases as unknown[] | undefined) ?? []) {
    const stair = object(raw, 'staircase');
    for (const key of ['centreX','startY','endY','startZ','endZ','width']) {
      if (!finite(stair[key])) throw new Error('Invalid staircase');
    }
    if ((stair.width as number) <= 0 || stair.startY === stair.endY) throw new Error('Invalid staircase dimensions');
  }

  for (const raw of level.lights as unknown[]) {
    const light = object(raw, 'light');
    if (!nonEmpty(light.id) || light.type !== 'point' || !vec3(light.position)) throw new Error('Invalid point light');
  }

  const entityIds = new Set<string>();
  for (const raw of level.entities as unknown[]) {
    const entity = object(raw, 'entity');
    if (!nonEmpty(entity.id) || entityIds.has(entity.id)) throw new Error('Invalid entity id');
    entityIds.add(entity.id);
    const components = object(entity.components, 'entity components');
    const transform = object(components.transform, 'entity transform');
    if (!vec3(transform.position) || (transform.forward !== undefined && !vec3(transform.forward))) {
      throw new Error(`Entity ${entity.id} transform is invalid`);
    }

    let collider: Record<string, unknown> | undefined;
    if (components.collider !== undefined) {
      collider = object(components.collider, 'collider component');
      if (collider.type !== 'box' || !vec3(collider.minimum) || !vec3(collider.maximum)) {
        throw new Error(`Entity ${entity.id} collider is invalid`);
      }
      if ((collider.minimum as Vec3Tuple).some((value, index) => value >= (collider!.maximum as Vec3Tuple)[index])) {
        throw new Error(`Entity ${entity.id} collider is empty`);
      }
      for (const key of ['collisionTags', 'mustCollideWith']) {
        if (collider[key] !== undefined &&
            (!Array.isArray(collider[key]) || !(collider[key] as unknown[]).every(nonEmpty))) {
          throw new Error(`Entity ${entity.id} collider filters are invalid`);
        }
      }
    }

    const hasCharacter = components.character !== undefined;
    const hasDynamicBody = components.dynamicBody !== undefined;
    if (hasCharacter === hasDynamicBody) {
      throw new Error(`Entity ${entity.id} must have exactly one runtime body component`);
    }
    if (hasCharacter) {
      object(components.character, 'character component');
    } else {
      if (!collider) throw new Error(`Dynamic entity ${entity.id} requires a collider`);
      const body = object(components.dynamicBody, 'dynamic body component');
      if (body.surfaceTextureMode !== undefined &&
          !['stretch','tile-local','tile-world'].includes(String(body.surfaceTextureMode))) {
        throw new Error(`Dynamic entity ${entity.id} texture mode is invalid`);
      }
      if (body.textureWorldUnitsPerTile !== undefined &&
          (!finite(body.textureWorldUnitsPerTile) || body.textureWorldUnitsPerTile <= 0)) {
        throw new Error(`Dynamic entity ${entity.id} texture tile size is invalid`);
      }
    }
  }
  return level as LevelDocument;
}

export function validateWorldDocument(value: unknown): WorldDocument {
  const world = object(value, 'world');
  if (world.schemaVersion !== CURRENT_SCHEMA_VERSION || !nonEmpty(world.id) ||
      !Array.isArray(world.levels) || world.levels.length === 0) throw new Error('World header is invalid');
  const settings = object(world.settings, 'world settings');
  if (!nonEmpty(settings.defaultLevel)) throw new Error('World defaultLevel is invalid');
  const ids = new Set<string>();
  for (const raw of world.levels) {
    const reference = object(raw, 'level reference');
    if (!nonEmpty(reference.id) || ids.has(reference.id) || !nonEmpty(reference.path) || !safePackagePath(reference.path)) {
      throw new Error('World level reference is invalid');
    }
    ids.add(reference.id);
  }
  if (!ids.has(settings.defaultLevel)) throw new Error('World defaultLevel is missing');
  materials(world.materials ?? {}, 'world materials');

  if (world.behaviours !== undefined) {
    if (!Array.isArray(world.behaviours)) throw new Error('World behaviours must be an array');
    const behaviourIds = new Set<string>();
    const faces = new Set(['any','left','right','back','front','bottom','top']);
    for (const raw of world.behaviours as unknown[]) {
      const behaviour = object(raw, 'world behaviour');
      if (!nonEmpty(behaviour.id) || behaviourIds.has(behaviour.id)) throw new Error('Invalid behaviour id');
      behaviourIds.add(behaviour.id);
      if (!nonEmpty(behaviour.type)) throw new Error(`Behaviour ${behaviour.id} type is invalid`);
      switch (behaviour.type) {
        case 'oscillating-gate':
          if (!nonEmpty(behaviour.leftEntity) || !nonEmpty(behaviour.rightEntity) || !vec3(behaviour.base)) {
            throw new Error(`Behaviour ${behaviour.id} gate references are invalid`);
          }
          for (const key of ['halfSpan','gap','sweep','angularSpeed','halfThickness','height']) {
            if (!finite(behaviour[key])) throw new Error(`Behaviour ${behaviour.id} has invalid ${key}`);
          }
          break;
        case 'vertical-cycle':
          if (!nonEmpty(behaviour.entity) || !vec3(behaviour.base)) throw new Error(`Behaviour ${behaviour.id} is invalid`);
          for (const key of ['upZ','downZ','period']) if (!finite(behaviour[key])) throw new Error(`Behaviour ${behaviour.id} is invalid`);
          if ((behaviour.period as number) <= 0) throw new Error(`Behaviour ${behaviour.id} period is invalid`);
          if (behaviour.lethalFace !== undefined && !faces.has(String(behaviour.lethalFace))) throw new Error(`Behaviour ${behaviour.id} face is invalid`);
          if (behaviour.contactTolerance !== undefined && (!finite(behaviour.contactTolerance) || behaviour.contactTolerance < 0)) throw new Error(`Behaviour ${behaviour.id} tolerance is invalid`);
          break;
        case 'rotation':
          if (!nonEmpty(behaviour.entity) || !finite(behaviour.angularSpeed) ||
              (behaviour.directionMultiplier !== undefined && !finite(behaviour.directionMultiplier))) {
            throw new Error(`Behaviour ${behaviour.id} is invalid`);
          }
          break;
        case 'hazard':
          if (!nonEmpty(behaviour.entity)) throw new Error(`Behaviour ${behaviour.id} entity is invalid`);
          if (behaviour.face !== undefined && !faces.has(String(behaviour.face))) throw new Error(`Behaviour ${behaviour.id} face is invalid`);
          if (behaviour.tolerance !== undefined && (!finite(behaviour.tolerance) || behaviour.tolerance < 0)) throw new Error(`Behaviour ${behaviour.id} tolerance is invalid`);
          if (behaviour.action !== undefined && behaviour.action !== 'respawn') throw new Error(`Behaviour ${behaviour.id} action is unsupported`);
          break;
        default:
          throw new Error(`Unsupported behaviour type ${String(behaviour.type)}`);
      }
    }
  }
  return world as WorldDocument;
}

export function validateLoadedWorldPackage(data: LoadedWorldPackage): LoadedWorldPackage {
  if (data.manifest.id !== data.world.id) throw new Error('Manifest and world ids disagree');
  if (data.levels.length !== data.world.levels.length) throw new Error('World level count mismatch');
  const ids = new Set(data.levels.map(level => level.id));
  for (let index = 0; index < data.levels.length; ++index) {
    if (data.levels[index].id !== data.world.levels[index].id) throw new Error('World level order mismatch');
  }
  for (const connector of data.world.connectors ?? []) {
    if (!connector.id || !ids.has(connector.fromLevel) || !ids.has(connector.toLevel) ||
        !vec3(connector.fromPosition) || !vec3(connector.toPosition)) {
      throw new Error(`Invalid connector ${connector.id || '<unnamed>'}`);
    }
    for (const sample of connector.forwardTraversal ?? []) if (!vec3(sample)) throw new Error('Invalid connector traversal');
    for (const sample of connector.reverseTraversal ?? []) if (!vec3(sample)) throw new Error('Invalid connector traversal');
  }

  const entityIds = new Set<string>();
  for (const level of data.levels) {
    for (const entity of level.entities) {
      if (entityIds.has(entity.id)) throw new Error(`Duplicate world entity id ${entity.id}`);
      entityIds.add(entity.id);
    }
  }
  for (const behaviour of data.world.behaviours ?? []) {
    const referenced = behaviour.type === 'oscillating-gate'
      ? [behaviour.leftEntity, behaviour.rightEntity]
      : [behaviour.entity];
    for (const id of referenced) {
      if (!entityIds.has(id)) throw new Error(`Behaviour ${behaviour.id} references missing entity ${id}`);
    }
  }
  return data;
}
