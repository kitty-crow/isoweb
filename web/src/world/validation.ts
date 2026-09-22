import type {
  AssetSourceDefinition, LevelDocument, LoadedWorldPackage, MaterialDefinition,
  PackageManifest, PrefabDefinition, Vec3Tuple, WorldDocument
} from './documents';
import {
  collectLevelResourceIds, collectWorldDeclaredAssetIds, requireEmbeddedResources
} from './PackageAssets';
import { CURRENT_SCHEMA_VERSION } from './schemas/version';

export { CURRENT_SCHEMA_VERSION } from './schemas/version';

const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);
const nonEmpty = (value: unknown): value is string => typeof value === 'string' && value.length > 0;
const vec3 = (value: unknown): value is Vec3Tuple =>
  Array.isArray(value) && value.length === 3 && value.every(finite);
const stringArray = (value: unknown): value is string[] =>
  Array.isArray(value) && value.every(nonEmpty);
const safePackagePath = (value: string): boolean =>
  !!value && !value.includes('\\') && !value.includes('\0') && !value.startsWith('/') &&
  !/^[A-Za-z]:/.test(value) && value.split('/').every(part => part !== '..' && part !== '');

function object(value: unknown, label: string): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} must be an object`);
  }
  return value as Record<string, unknown>;
}

function validateTransform(value: unknown, label: string): void {
  const transform = object(value, label);
  if (!vec3(transform.position) ||
      (transform.rotation !== undefined && !vec3(transform.rotation)) ||
      (transform.forward !== undefined && !vec3(transform.forward)) ||
      (transform.scale !== undefined && (
        !vec3(transform.scale) || (transform.scale as Vec3Tuple).some(component => component === 0)
      ))) {
    throw new Error(`${label} is invalid`);
  }
}

function validateEditorMetadata(value: unknown, label: string): void {
  if (value === undefined) return;
  const editor = object(value, label);
  for (const key of ['hiddenIds', 'lockedIds']) {
    if (editor[key] !== undefined && !stringArray(editor[key])) {
      throw new Error(`${label}.${key} must be an array of ids`);
    }
  }
  if (editor.metadata !== undefined) object(editor.metadata, `${label}.metadata`);
}

export function validateManifest(value: unknown, expected: PackageManifest['format']): PackageManifest {
  const manifest = object(value, 'manifest');
  if (manifest.format !== expected) throw new Error(`Expected ${expected} package`);
  if (manifest.representation !== undefined &&
      manifest.representation !== 'source' && manifest.representation !== 'compiled') {
    throw new Error(`Unsupported package representation ${String(manifest.representation)}`);
  }
  if (manifest.schemaVersion !== CURRENT_SCHEMA_VERSION) {
    throw new Error(`Unsupported manifest schema version ${String(manifest.schemaVersion)}`);
  }
  if (!nonEmpty(manifest.id)) throw new Error('Manifest id must be non-empty');
  if (!nonEmpty(manifest.entry) || !safePackagePath(manifest.entry)) {
    throw new Error('Manifest entry is unsafe');
  }
  if (manifest.assets !== undefined) {
    if (!Array.isArray(manifest.assets)) throw new Error('Manifest assets must be an array');
    const assetIds = new Set<string>();
    const assetPaths = new Set<string>();
    for (const raw of manifest.assets as unknown[]) {
      const asset = object(raw, 'manifest asset');
      if (!nonEmpty(asset.id) || assetIds.has(asset.id)) throw new Error('Manifest asset id is invalid');
      if (!nonEmpty(asset.path) || !safePackagePath(asset.path) || assetPaths.has(asset.path)) {
        throw new Error(`Manifest asset path is invalid for ${String(asset.id)}`);
      }
      if (asset.mediaType !== undefined && !nonEmpty(asset.mediaType)) {
        throw new Error(`Manifest asset media type is invalid for ${asset.id}`);
      }
      if (asset.size !== undefined &&
          (!Number.isSafeInteger(asset.size) || (asset.size as number) < 0)) {
        throw new Error(`Manifest asset size is invalid for ${asset.id}`);
      }
      if (asset.hash !== undefined && !nonEmpty(asset.hash)) {
        throw new Error(`Manifest asset hash is invalid for ${asset.id}`);
      }
      assetIds.add(asset.id);
      assetPaths.add(asset.path);
    }
  }
  return manifest as PackageManifest;
}

function assetSources(value: unknown, label: string): Record<string, AssetSourceDefinition> {
  const registry = object(value ?? {}, label);
  for (const [id, raw] of Object.entries(registry)) {
    if (!nonEmpty(id)) throw new Error(`${label} has an empty asset id`);
    const asset = object(raw, `${label}.${id}`);
    if (!nonEmpty(asset.source) || !safePackagePath(asset.source)) {
      throw new Error(`Asset ${id} has an unsafe source path`);
    }
    if (asset.mediaType !== undefined && !nonEmpty(asset.mediaType)) {
      throw new Error(`Asset ${id} has an invalid media type`);
    }
    if (asset.provenance !== undefined) {
      const provenance = object(asset.provenance, `${label}.${id}.provenance`);
      for (const key of ['sourceUrl', 'author', 'licence', 'licenceUrl', 'attribution']) {
        if (provenance[key] !== undefined && !nonEmpty(provenance[key])) {
          throw new Error(`Asset ${id} has invalid provenance ${key}`);
        }
      }
      if (provenance.redistributable !== undefined &&
          !['yes', 'no', 'unknown'].includes(String(provenance.redistributable))) {
        throw new Error(`Asset ${id} has invalid redistribution metadata`);
      }
    }
  }
  return registry as Record<string, AssetSourceDefinition>;
}

function materials(value: unknown, label: string): Record<string, MaterialDefinition> {
  const registry = object(value, label);
  for (const [id, raw] of Object.entries(registry)) {
    const material = object(raw, `${label}.${id}`);
    if (material.id !== id || !vec3(material.baseColour)) throw new Error(`Invalid material ${id}`);
    if (material.opacity !== undefined &&
        (!finite(material.opacity) || (material.opacity as number) < 0 || (material.opacity as number) > 1)) {
      throw new Error(`Material ${id} has invalid opacity`);
    }
    if (material.alphaMode !== undefined &&
        !['opaque', 'mask', 'blend'].includes(String(material.alphaMode))) {
      throw new Error(`Material ${id} has invalid alpha mode`);
    }
    if (material.baseColourTexture !== undefined && !nonEmpty(material.baseColourTexture)) {
      throw new Error(`Material ${id} has invalid texture id`);
    }
    if (material.textureMode !== undefined &&
        !['stretch', 'tile-local', 'tile-world'].includes(String(material.textureMode))) {
      throw new Error(`Material ${id} has invalid texture mode`);
    }
    if (material.worldUnitsPerTile !== undefined &&
        (!finite(material.worldUnitsPerTile) || (material.worldUnitsPerTile as number) <= 0)) {
      throw new Error(`Material ${id} has invalid worldUnitsPerTile`);
    }
    if (material.emissive !== undefined && !vec3(material.emissive)) {
      throw new Error(`Material ${id} has invalid emissive colour`);
    }
  }
  return registry as Record<string, MaterialDefinition>;
}

function validateMaterialAssetReferences(
  registry: Record<string, MaterialDefinition>,
  assets: Record<string, AssetSourceDefinition>,
  label: string
): void {
  for (const material of Object.values(registry)) {
    if (material.baseColourTexture && !assets[material.baseColourTexture]) {
      throw new Error(`${label} material ${material.id} references missing asset ${material.baseColourTexture}`);
    }
  }
}

function validateEntity(
  raw: unknown,
  entityIds: Set<string>,
  assets: Record<string, AssetSourceDefinition>,
  label: string
): void {
  const entity = object(raw, label);
  if (!nonEmpty(entity.id) || entityIds.has(entity.id)) throw new Error(`Invalid entity id in ${label}`);
  entityIds.add(entity.id);
  const components = object(entity.components, `${label} components`);
  validateTransform(components.transform, `${label} ${entity.id} transform`);

  let collider: Record<string, unknown> | undefined;
  if (components.collider !== undefined) {
    collider = object(components.collider, `${label} collider component`);
    if (collider.type !== 'box' || !vec3(collider.minimum) || !vec3(collider.maximum)) {
      throw new Error(`Entity ${entity.id} collider is invalid`);
    }
    if ((collider.minimum as Vec3Tuple).some(
      (value, index) => value >= (collider!.maximum as Vec3Tuple)[index]
    )) {
      throw new Error(`Entity ${entity.id} collider is empty`);
    }
    for (const key of ['collisionTags', 'mustCollideWith']) {
      if (collider[key] !== undefined && !stringArray(collider[key])) {
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
    const character = object(components.character, `${label} character component`);
    if (character.sprites !== undefined) {
      const sprites = object(character.sprites, `${label} character sprites`);
      for (const [state, rawDirectional] of Object.entries(sprites)) {
        if (state !== 'still' && state !== 'moving' && state !== 'actions') {
          throw new Error(`Entity ${entity.id} has unsupported sprite state ${state}`);
        }
        const directionals = state === 'actions'
          ? Object.values(object(rawDirectional, `Entity ${entity.id} action sprites`))
          : [rawDirectional];
        for (const rawDirections of directionals) {
          const directions = object(rawDirections, `Entity ${entity.id} directional sprites`);
          for (const [facing, rawAnimation] of Object.entries(directions)) {
            if (!['front', 'back', 'left', 'right'].includes(facing)) {
              throw new Error(`Entity ${entity.id} has unsupported sprite facing ${facing}`);
            }
            const animation = object(rawAnimation, `Entity ${entity.id} sprite animation`);
            if (!nonEmpty(animation.resource) || !assets[animation.resource]) {
              throw new Error(
                `Entity ${entity.id} references undeclared asset ${String(animation.resource)}`
              );
            }
            for (const key of ['frameCount', 'columns', 'rows', 'fps', 'worldWidth', 'worldHeight']) {
              if (animation[key] !== undefined && !finite(animation[key])) {
                throw new Error(`Entity ${entity.id} sprite ${key} is invalid`);
              }
            }
            if (animation.loop !== undefined && typeof animation.loop !== 'boolean') {
              throw new Error(`Entity ${entity.id} sprite loop is invalid`);
            }
          }
        }
      }
    }
  } else {
    if (!collider) throw new Error(`Dynamic entity ${entity.id} requires a collider`);
    const body = object(components.dynamicBody, `${label} dynamic body component`);
    if (body.surfaceTextureMode !== undefined &&
        !['stretch', 'tile-local', 'tile-world'].includes(String(body.surfaceTextureMode))) {
      throw new Error(`Dynamic entity ${entity.id} texture mode is invalid`);
    }
    if (body.textureWorldUnitsPerTile !== undefined &&
        (!finite(body.textureWorldUnitsPerTile) || (body.textureWorldUnitsPerTile as number) <= 0)) {
      throw new Error(`Dynamic entity ${entity.id} texture tile size is invalid`);
    }
  }
}

function prefabs(
  value: unknown,
  label: string,
  assets: Record<string, AssetSourceDefinition>
): Record<string, PrefabDefinition> {
  const registry = object(value ?? {}, label);
  for (const [id, raw] of Object.entries(registry)) {
    const prefab = object(raw, `${label}.${id}`);
    if (prefab.id !== id || !Array.isArray(prefab.entities)) throw new Error(`Invalid prefab ${id}`);
    const entityIds = new Set<string>();
    for (const entity of prefab.entities as unknown[]) {
      validateEntity(entity, entityIds, assets, `Prefab ${id}`);
    }
    if (prefab.metadata !== undefined) object(prefab.metadata, `Prefab ${id} metadata`);
  }
  return registry as Record<string, PrefabDefinition>;
}

function validateLocalConnectors(value: unknown[], levelId: string): void {
  const ids = new Set<string>();
  for (const raw of value) {
    const connector = object(raw, `Level ${levelId} connector`);
    if (!nonEmpty(connector.id) || ids.has(connector.id) || !nonEmpty(connector.type) ||
        !vec3(connector.fromPosition) || !vec3(connector.toPosition)) {
      throw new Error(`Invalid local connector in level ${levelId}`);
    }
    ids.add(connector.id);
    for (const key of ['forwardTraversal', 'reverseTraversal']) {
      if (connector[key] !== undefined &&
          (!Array.isArray(connector[key]) || !(connector[key] as unknown[]).every(vec3))) {
        throw new Error(`Connector ${connector.id} has invalid traversal samples`);
      }
    }
    if (connector.bidirectional !== undefined && typeof connector.bidirectional !== 'boolean') {
      throw new Error(`Connector ${connector.id} has invalid bidirectional flag`);
    }
  }
}

function validateLevelBehaviours(
  value: unknown,
  label: string,
  entityIds: Set<string>
): void {
  if (value === undefined) return;
  if (!Array.isArray(value)) throw new Error(`${label} behaviours must be an array`);
  const ids = new Set<string>();
  const faces = new Set(['any', 'left', 'right', 'back', 'front', 'bottom', 'top']);

  for (const raw of value) {
    const behaviour = object(raw, `${label} behaviour`);
    if (!nonEmpty(behaviour.id) || ids.has(behaviour.id)) {
      throw new Error(`${label} has an invalid behaviour id`);
    }
    ids.add(behaviour.id);

    const requireEntity = (id: unknown): string => {
      if (!nonEmpty(id) || !entityIds.has(id)) {
        throw new Error(`Behaviour ${behaviour.id} references missing entity ${String(id)}`);
      }
      return id;
    };

    switch (behaviour.type) {
      case 'vertical-cycle':
        requireEntity(behaviour.entity);
        if (!vec3(behaviour.base) ||
            !finite(behaviour.upZ) || !finite(behaviour.downZ) ||
            !finite(behaviour.period) || (behaviour.period as number) <= 0) {
          throw new Error(`Behaviour ${behaviour.id} is invalid`);
        }
        if (behaviour.lethalFace !== undefined && !faces.has(String(behaviour.lethalFace))) {
          throw new Error(`Behaviour ${behaviour.id} face is invalid`);
        }
        break;
      case 'rotation':
        requireEntity(behaviour.entity);
        if (!finite(behaviour.angularSpeed) ||
            (behaviour.directionMultiplier !== undefined && !finite(behaviour.directionMultiplier))) {
          throw new Error(`Behaviour ${behaviour.id} is invalid`);
        }
        break;
      case 'hazard':
        requireEntity(behaviour.entity);
        if (behaviour.face !== undefined && !faces.has(String(behaviour.face))) {
          throw new Error(`Behaviour ${behaviour.id} face is invalid`);
        }
        if (behaviour.action !== undefined && behaviour.action !== 'respawn') {
          throw new Error(`Behaviour ${behaviour.id} action is unsupported`);
        }
        break;
      case 'oscillating-gate':
        requireEntity(behaviour.leftEntity);
        requireEntity(behaviour.rightEntity);
        if (!vec3(behaviour.base)) throw new Error(`Behaviour ${behaviour.id} is invalid`);
        for (const key of ['halfSpan', 'gap', 'sweep', 'angularSpeed', 'halfThickness', 'height']) {
          if (!finite(behaviour[key])) throw new Error(`Behaviour ${behaviour.id} is invalid`);
        }
        break;
      default:
        throw new Error(`Unsupported behaviour type ${String(behaviour.type)}`);
    }
  }
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
  const levelAssets = assetSources(level.assets ?? {}, `Level ${level.id} assets`);
  validateMaterialAssetReferences(registry, levelAssets, `Level ${level.id}`);
  prefabs(level.localPrefabs ?? {}, `Level ${level.id} prefabs`, levelAssets);
  validateEditorMetadata(level.editor, `Level ${level.id} editor metadata`);
  const settings = object(level.settings, `Level ${level.id} settings`);
  if (!vec3(settings.boundsFocus)) throw new Error(`Level ${level.id} boundsFocus is invalid`);
  for (const key of ['floorDarkMaterial', 'floorLightMaterial', 'wallMaterial']) {
    const id = settings[key];
    if (!nonEmpty(id) || !registry[id]) throw new Error(`Level ${level.id} references missing material`);
  }

  const groundIds = new Set<string>();
  for (const raw of level.ground as unknown[]) {
    const ground = object(raw, 'ground');
    if (!nonEmpty(ground.id) || groundIds.has(ground.id) ||
        ground.type !== 'rectangle' || !vec3(ground.centre)) {
      throw new Error(`Invalid ground in level ${level.id}`);
    }
    groundIds.add(ground.id);
    if (!Array.isArray(ground.size) || ground.size.length !== 2 || !ground.size.every(finite) ||
        ground.size.some(size => size <= 0)) {
      throw new Error(`Invalid ground size ${ground.id}`);
    }
    if (!nonEmpty(ground.material) || !registry[ground.material]) {
      throw new Error(`Missing ground material ${ground.id}`);
    }
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
    if ((room.width as number) <= 0 || (room.depth as number) <= 0) {
      throw new Error(`Room ${room.id} is empty`);
    }
    if (!nonEmpty(room.wallMaterial) || !registry[room.wallMaterial]) {
      throw new Error(`Room ${room.id} material is missing`);
    }
  }

  const connectionIds = new Set<string>();
  for (const raw of (level.roomConnections as unknown[] | undefined) ?? []) {
    const connection = object(raw, 'room connection');
    if (!nonEmpty(connection.id) || connectionIds.has(connection.id)) {
      throw new Error('Invalid room connection id');
    }
    connectionIds.add(connection.id);
    for (const endpointKey of ['a', 'b']) {
      const endpoint = object(connection[endpointKey], 'room portal');
      if (!nonEmpty(endpoint.roomId) || !roomIds.has(endpoint.roomId) ||
          !['north', 'south', 'east', 'west'].includes(String(endpoint.side)) ||
          !finite(endpoint.width) || (endpoint.width as number) <= 0 ||
          (endpoint.offset !== undefined && !finite(endpoint.offset))) {
        throw new Error(`Room connection ${connection.id} has invalid portal`);
      }
    }
  }

  const geometryIds = new Set<string>();
  const primitives = new Set(['cube', 'sphere', 'cone', 'pyramid', 'dodecahedron', 'icosahedron']);
  for (const raw of level.geometry as unknown[]) {
    const geometry = object(raw, 'geometry');
    if (!nonEmpty(geometry.id) || geometryIds.has(geometry.id) ||
        !primitives.has(String(geometry.type)) || !vec3(geometry.position) ||
        !finite(geometry.size) || (geometry.size as number) <= 0 ||
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
        (hole.minimum[0] as number) >= (hole.maximum[0] as number) ||
        (hole.minimum[1] as number) >= (hole.maximum[1] as number)) {
      throw new Error('Invalid floor hole');
    }
  }

  for (const raw of (level.staircases as unknown[] | undefined) ?? []) {
    const stair = object(raw, 'staircase');
    for (const key of ['centreX', 'startY', 'endY', 'startZ', 'endZ', 'width']) {
      if (!finite(stair[key])) throw new Error('Invalid staircase');
    }
    if ((stair.width as number) <= 0 || stair.startY === stair.endY) {
      throw new Error('Invalid staircase dimensions');
    }
  }

  const lightIds = new Set<string>();
  for (const raw of level.lights as unknown[]) {
    const light = object(raw, 'light');
    if (!nonEmpty(light.id) || lightIds.has(light.id) ||
        light.type !== 'point' || !vec3(light.position)) {
      throw new Error('Invalid point light');
    }
    lightIds.add(light.id);
  }

  const entityIds = new Set<string>();
  for (const raw of level.entities as unknown[]) {
    validateEntity(raw, entityIds, levelAssets, `Level ${level.id}`);
  }

  const spawnIds = new Set<string>();
  for (const raw of level.spawns as unknown[]) {
    const spawn = object(raw, `Level ${level.id} spawn`);
    if (!nonEmpty(spawn.id) || spawnIds.has(spawn.id)) {
      throw new Error(`Invalid spawn id in level ${level.id}`);
    }
    spawnIds.add(spawn.id);
    validateTransform(spawn.transform, `Spawn ${spawn.id} transform`);
    if (spawn.entityId !== undefined &&
        (!nonEmpty(spawn.entityId) || !entityIds.has(spawn.entityId))) {
      throw new Error(`Spawn ${spawn.id} references missing entity ${String(spawn.entityId)}`);
    }
    if (spawn.tags !== undefined && !stringArray(spawn.tags)) {
      throw new Error(`Spawn ${spawn.id} tags are invalid`);
    }
  }

  validateLocalConnectors(level.connectors as unknown[], level.id as string);
  validateLevelBehaviours(level.behaviours, `Level ${level.id}`, entityIds);
  return level as LevelDocument;
}

export function validateWorldDocument(value: unknown): WorldDocument {
  const world = object(value, 'world');
  if (world.schemaVersion !== CURRENT_SCHEMA_VERSION || !nonEmpty(world.id) ||
      !Array.isArray(world.levels) || world.levels.length === 0) {
    throw new Error('World header is invalid');
  }
  const settings = object(world.settings, 'world settings');
  if (!nonEmpty(settings.defaultLevel)) throw new Error('World defaultLevel is invalid');
  const ids = new Set<string>();
  for (const raw of world.levels) {
    const reference = object(raw, 'level reference');
    if (!nonEmpty(reference.id) || ids.has(reference.id) ||
        !nonEmpty(reference.path) || !safePackagePath(reference.path)) {
      throw new Error('World level reference is invalid');
    }
    ids.add(reference.id);
  }
  if (!ids.has(settings.defaultLevel)) throw new Error('World defaultLevel is missing');

  const worldMaterials = materials(world.materials ?? {}, 'world materials');
  const worldAssets = assetSources(world.assets ?? {}, 'world assets');
  validateMaterialAssetReferences(worldMaterials, worldAssets, `World ${world.id}`);
  prefabs(world.prefabs ?? {}, 'world prefabs', worldAssets);
  validateEditorMetadata(world.editor, `World ${world.id} editor metadata`);

  if (world.behaviours !== undefined) {
    if (!Array.isArray(world.behaviours)) throw new Error('World behaviours must be an array');
    const behaviourIds = new Set<string>();
    const faces = new Set(['any', 'left', 'right', 'back', 'front', 'bottom', 'top']);
    for (const raw of world.behaviours as unknown[]) {
      const behaviour = object(raw, 'world behaviour');
      if (!nonEmpty(behaviour.id) || behaviourIds.has(behaviour.id)) {
        throw new Error('Invalid behaviour id');
      }
      behaviourIds.add(behaviour.id);
      if (!nonEmpty(behaviour.type)) throw new Error(`Behaviour ${behaviour.id} type is invalid`);
      switch (behaviour.type) {
        case 'oscillating-gate':
          if (!nonEmpty(behaviour.leftEntity) || !nonEmpty(behaviour.rightEntity) ||
              !vec3(behaviour.base)) {
            throw new Error(`Behaviour ${behaviour.id} gate references are invalid`);
          }
          for (const key of ['halfSpan', 'gap', 'sweep', 'angularSpeed', 'halfThickness', 'height']) {
            if (!finite(behaviour[key])) {
              throw new Error(`Behaviour ${behaviour.id} has invalid ${key}`);
            }
          }
          break;
        case 'vertical-cycle':
          if (!nonEmpty(behaviour.entity) || !vec3(behaviour.base)) {
            throw new Error(`Behaviour ${behaviour.id} is invalid`);
          }
          for (const key of ['upZ', 'downZ', 'period']) {
            if (!finite(behaviour[key])) throw new Error(`Behaviour ${behaviour.id} is invalid`);
          }
          if ((behaviour.period as number) <= 0) {
            throw new Error(`Behaviour ${behaviour.id} period is invalid`);
          }
          if (behaviour.lethalFace !== undefined &&
              !faces.has(String(behaviour.lethalFace))) {
            throw new Error(`Behaviour ${behaviour.id} face is invalid`);
          }
          if (behaviour.contactTolerance !== undefined &&
              (!finite(behaviour.contactTolerance) || (behaviour.contactTolerance as number) < 0)) {
            throw new Error(`Behaviour ${behaviour.id} tolerance is invalid`);
          }
          break;
        case 'rotation':
          if (!nonEmpty(behaviour.entity) || !finite(behaviour.angularSpeed) ||
              (behaviour.directionMultiplier !== undefined && !finite(behaviour.directionMultiplier))) {
            throw new Error(`Behaviour ${behaviour.id} is invalid`);
          }
          break;
        case 'hazard':
          if (!nonEmpty(behaviour.entity)) {
            throw new Error(`Behaviour ${behaviour.id} entity is invalid`);
          }
          if (behaviour.face !== undefined && !faces.has(String(behaviour.face))) {
            throw new Error(`Behaviour ${behaviour.id} face is invalid`);
          }
          if (behaviour.tolerance !== undefined &&
              (!finite(behaviour.tolerance) || (behaviour.tolerance as number) < 0)) {
            throw new Error(`Behaviour ${behaviour.id} tolerance is invalid`);
          }
          if (behaviour.action !== undefined && behaviour.action !== 'respawn') {
            throw new Error(`Behaviour ${behaviour.id} action is unsupported`);
          }
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
    if (data.levels[index].id !== data.world.levels[index].id) {
      throw new Error('World level order mismatch');
    }
  }

  const connectorIds = new Set<string>();
  for (const connector of data.world.connectors ?? []) {
    if (!connector.id || connectorIds.has(connector.id) ||
        !ids.has(connector.fromLevel) || !ids.has(connector.toLevel) ||
        !vec3(connector.fromPosition) || !vec3(connector.toPosition)) {
      throw new Error(`Invalid connector ${connector.id || '<unnamed>'}`);
    }
    connectorIds.add(connector.id);
    for (const sample of connector.forwardTraversal ?? []) {
      if (!vec3(sample)) throw new Error('Invalid connector traversal');
    }
    for (const sample of connector.reverseTraversal ?? []) {
      if (!vec3(sample)) throw new Error('Invalid connector traversal');
    }
  }

  const entityIds = new Set<string>();
  for (const level of data.levels) {
    for (const entity of level.entities) {
      if (entityIds.has(entity.id)) throw new Error(`Duplicate world entity id ${entity.id}`);
      entityIds.add(entity.id);
    }
  }
  for (const level of data.levels) {
    requireEmbeddedResources(
      collectLevelResourceIds(level),
      data.assets,
      `World ${data.world.id} level ${level.id}`
    );
  }
  requireEmbeddedResources(
    collectWorldDeclaredAssetIds(data.world.assets),
    data.assets,
    `World ${data.world.id}`
  );
  const runtimeBehaviours = [
    ...(data.world.behaviours ?? []),
    ...data.levels.flatMap(level => level.behaviours ?? [])
  ];
  const runtimeBehaviourIds = new Set<string>();
  for (const behaviour of runtimeBehaviours) {
    if (runtimeBehaviourIds.has(behaviour.id)) {
      throw new Error(`Duplicate world behaviour id ${behaviour.id}`);
    }
    runtimeBehaviourIds.add(behaviour.id);
    const referenced = behaviour.type === 'oscillating-gate'
      ? [behaviour.leftEntity, behaviour.rightEntity]
      : [behaviour.entity];
    for (const id of referenced) {
      if (!entityIds.has(id)) {
        throw new Error(`Behaviour ${behaviour.id} references missing entity ${id}`);
      }
    }
  }
  return data;
}
