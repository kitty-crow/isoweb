import type {
  CompiledBehaviour, CompiledEntity, CompiledLevelDocument, CompiledWorldDocument
} from './WorldCompiler';
import { COMPILED_FORMAT_VERSION } from './WorldCompiler';
import { CURRENT_SCHEMA_VERSION } from './validation';

const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);
const nonEmpty = (value: unknown): value is string => typeof value === 'string' && value.length > 0;
const vec3 = (value: unknown): value is [number, number, number] =>
  Array.isArray(value) && value.length === 3 && value.every(finite);

function object(value: unknown, label: string): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as Record<string, unknown>;
}

function integerIn(value: unknown, minimum: number, maximum: number): boolean {
  return Number.isInteger(value) && Number(value) >= minimum && Number(value) <= maximum;
}

function stringArray(value: unknown): boolean {
  return Array.isArray(value) && value.every(nonEmpty);
}

function validateCompiledEntity(value: unknown, levelId: string): CompiledEntity {
  const entity = object(value, 'compiled entity');
  if (!nonEmpty(entity.id) || !nonEmpty(entity.kind) || !vec3(entity.position)) {
    throw new Error(`Invalid compiled entity in ${levelId}`);
  }
  if (!Array.isArray(entity.forward) || entity.forward.length !== 2 || !entity.forward.every(finite) ||
      !vec3(entity.hitBoxMinimum) || !vec3(entity.hitBoxMaximum)) {
    throw new Error(`Compiled entity ${entity.id} transform/collider is invalid`);
  }
  if ((entity.hitBoxMinimum as number[]).some((v, i) => v >= (entity.hitBoxMaximum as number[])[i])) {
    throw new Error(`Compiled entity ${entity.id} collider is empty`);
  }
  if (![0, 1].includes(Number(entity.solid)) ||
      !stringArray(entity.collisionTags) || !stringArray(entity.mustCollideWith)) {
    throw new Error(`Compiled entity ${entity.id} flags are invalid`);
  }
  if (entity.kind === 'dynamic') {
    if (!integerIn(entity.textureMode, 0, 2) ||
        !finite(entity.textureWorldUnitsPerTile) || Number(entity.textureWorldUnitsPerTile) <= 0) {
      throw new Error(`Compiled dynamic entity ${entity.id} is invalid`);
    }
  } else if (entity.kind === 'character') {
    if (![0, 1].includes(Number(entity.npc)) || ![0, 1].includes(Number(entity.controllable)) ||
        !finite(entity.movementSpeedMultiplier) || !finite(entity.crouchedHeight) ||
        !Array.isArray(entity.sprites)) {
      throw new Error(`Compiled Character ${entity.id} is invalid`);
    }
    for (const raw of entity.sprites as unknown[]) {
      const sprite = object(raw, 'compiled sprite binding');
      const animation = object(sprite.animation, 'compiled sprite animation');
      if (!integerIn(sprite.state, 0, 2) || !integerIn(sprite.facing, 0, 3) ||
          typeof sprite.action !== 'string' || !nonEmpty(animation.resource)) {
        throw new Error(`Compiled Character ${entity.id} sprite is invalid`);
      }
      for (const key of ['frameCount','columns','rows','fps','worldWidth','worldHeight']) {
        if (!finite(animation[key])) throw new Error(`Compiled Character ${entity.id} sprite ${key} is invalid`);
      }
      if (typeof animation.loop !== 'boolean') throw new Error(`Compiled Character ${entity.id} sprite loop is invalid`);
    }
  } else {
    throw new Error(`Compiled entity ${entity.id} kind is unsupported`);
  }
  return entity as unknown as CompiledEntity;
}

export function validateCompiledLevelDocument(value: unknown): CompiledLevelDocument {
  const level = object(value, 'compiled level');
  if (level.schemaVersion !== CURRENT_SCHEMA_VERSION ||
      level.compiledFormatVersion !== COMPILED_FORMAT_VERSION ||
      !nonEmpty(level.id) || !vec3(level.viewOrigin) || !vec3(level.lightPosition) ||
      !vec3(level.floorDark) || !vec3(level.floorLight) || !vec3(level.wallColour) ||
      !vec3(level.boundsFocus)) {
    throw new Error('Compiled level header is invalid');
  }
  for (const key of ['ground','rooms','roomConnections','primitives','floorHoles','staircases','entities','assets']) {
    if (!Array.isArray(level[key])) throw new Error(`Compiled level ${level.id} ${key} must be an array`);
  }

  for (const raw of level.ground as unknown[]) {
    const item = object(raw, 'compiled ground');
    if (!vec3(item.centre) || !finite(item.width) || !finite(item.depth) ||
        Number(item.width) <= 0 || Number(item.depth) <= 0 || ![0,1].includes(Number(item.walkable))) {
      throw new Error(`Compiled level ${level.id} ground is invalid`);
    }
  }
  const roomIds = new Set<string>();
  for (const raw of level.rooms as unknown[]) {
    const room = object(raw, 'compiled room');
    if (!nonEmpty(room.id) || roomIds.has(room.id)) throw new Error('Compiled room id is invalid');
    roomIds.add(room.id);
    for (const key of ['centreX','centreY','floorZ','width','depth','wallHeight','wallThickness']) {
      if (!finite(room[key])) throw new Error(`Compiled room ${room.id} is invalid`);
    }
  }
  for (const raw of level.roomConnections as unknown[]) {
    const connection = object(raw, 'compiled room connection');
    if (!nonEmpty(connection.id)) throw new Error('Compiled room connection id is invalid');
    for (const endpointName of ['a','b']) {
      const endpoint = object(connection[endpointName], 'compiled room endpoint');
      if (!nonEmpty(endpoint.roomId) || !roomIds.has(String(endpoint.roomId)) ||
          !integerIn(endpoint.side, 0, 3) || !finite(endpoint.offset) || !finite(endpoint.width)) {
        throw new Error(`Compiled room connection ${connection.id} is invalid`);
      }
    }
    if (![0,1].includes(Number(connection.openPassage))) throw new Error(`Compiled room connection ${connection.id} flag is invalid`);
  }
  for (const raw of level.primitives as unknown[]) {
    const primitive = object(raw, 'compiled primitive');
    if (!integerIn(primitive.kind, 0, 5) || !vec3(primitive.position) || !finite(primitive.size) ||
        !finite(primitive.height) || !vec3(primitive.colour) || ![0,1].includes(Number(primitive.solid))) {
      throw new Error(`Compiled primitive in ${level.id} is invalid`);
    }
  }
  for (const raw of level.floorHoles as unknown[]) {
    const hole = object(raw, 'compiled floor hole');
    for (const key of ['minimumX','maximumX','minimumY','maximumY']) if (!finite(hole[key])) throw new Error('Compiled floor hole is invalid');
    if (Number(hole.minimumX) >= Number(hole.maximumX) || Number(hole.minimumY) >= Number(hole.maximumY)) throw new Error('Compiled floor hole is empty');
  }
  for (const raw of level.staircases as unknown[]) {
    const stair = object(raw, 'compiled staircase');
    for (const key of ['startX','startY','endX','endY','startZ','endZ','width']) {
      if (!finite(stair[key])) throw new Error('Compiled staircase is invalid');
    }
    if (Number(stair.width) <= 0 ||
        (stair.startX === stair.endX && stair.startY === stair.endY)) {
      throw new Error('Compiled staircase dimensions are invalid');
    }
  }

  if (!(level.assets as unknown[]).every(nonEmpty) ||
      new Set(level.assets as string[]).size !== (level.assets as string[]).length) {
    throw new Error(`Compiled level ${level.id} assets are invalid`);
  }

  const entityIds = new Set<string>();
  for (const raw of level.entities as unknown[]) {
    const entity = validateCompiledEntity(raw, String(level.id));
    if (entityIds.has(entity.id)) throw new Error(`Duplicate compiled entity ${entity.id}`);
    entityIds.add(entity.id);
  }
  return level as unknown as CompiledLevelDocument;
}

function validateCompiledBehaviour(value: unknown): CompiledBehaviour {
  const behaviour = object(value, 'compiled behaviour');
  if (!nonEmpty(behaviour.id) || !nonEmpty(behaviour.type)) throw new Error('Compiled behaviour header is invalid');
  switch (behaviour.type) {
    case 'oscillating-gate':
      if (!nonEmpty(behaviour.leftEntity) || !nonEmpty(behaviour.rightEntity) || !vec3(behaviour.base)) throw new Error(`Compiled gate ${behaviour.id} is invalid`);
      for (const key of ['halfSpan','gap','sweep','angularSpeed','halfThickness','height']) if (!finite(behaviour[key])) throw new Error(`Compiled gate ${behaviour.id} is invalid`);
      break;
    case 'vertical-cycle':
      if (!nonEmpty(behaviour.entity) || !vec3(behaviour.base) || ![0,1].includes(Number(behaviour.blockOnSafeContact)) ||
          !integerIn(behaviour.lethalFace,0,6)) throw new Error(`Compiled vertical cycle ${behaviour.id} is invalid`);
      for (const key of ['upZ','downZ','period','contactTolerance']) if (!finite(behaviour[key])) throw new Error(`Compiled vertical cycle ${behaviour.id} is invalid`);
      break;
    case 'rotation':
      if (!nonEmpty(behaviour.entity) || !finite(behaviour.angularSpeed) || !finite(behaviour.directionMultiplier)) throw new Error(`Compiled rotation ${behaviour.id} is invalid`);
      break;
    case 'hazard':
      if (!nonEmpty(behaviour.entity) || !integerIn(behaviour.face,0,6) || !finite(behaviour.tolerance) || behaviour.action !== 'respawn') throw new Error(`Compiled hazard ${behaviour.id} is invalid`);
      break;
    default:
      throw new Error(`Unsupported compiled behaviour ${String(behaviour.type)}`);
  }
  return behaviour as unknown as CompiledBehaviour;
}

export function validateCompiledWorldDocument(value: unknown): CompiledWorldDocument {
  const world = object(value, 'compiled world');
  if (world.schemaVersion !== CURRENT_SCHEMA_VERSION ||
      world.compiledFormatVersion !== COMPILED_FORMAT_VERSION ||
      !nonEmpty(world.id) || !Number.isInteger(world.defaultLevelIndex) ||
      !finite(world.lowerLevelPreviewDepth) || !finite(world.lowerPreviewResolutionScale) ||
      !Array.isArray(world.levels) || world.levels.length === 0 ||
      !Array.isArray(world.connectors) || !Array.isArray(world.behaviours) ||
      !Array.isArray(world.assets) || !(world.assets as unknown[]).every(nonEmpty) ||
      new Set(world.assets as string[]).size !== (world.assets as string[]).length) {
    throw new Error('Compiled world header is invalid');
  }
  if (Number(world.defaultLevelIndex) < 0 || Number(world.defaultLevelIndex) >= world.levels.length) {
    throw new Error('Compiled world default level index is invalid');
  }
  const ids = new Set<string>();
  for (const raw of world.levels as unknown[]) {
    const reference = object(raw, 'compiled level reference');
    if (!nonEmpty(reference.id) || ids.has(String(reference.id)) || !nonEmpty(reference.path)) throw new Error('Compiled level reference is invalid');
    ids.add(String(reference.id));
  }
  const engine = object(world.engine, 'compiled engine defaults');
  if (engine.baseMovementSpeed !== undefined && !finite(engine.baseMovementSpeed)) throw new Error('Compiled engine speed is invalid');
  if (engine.selectionMode !== undefined && !integerIn(engine.selectionMode,0,1)) throw new Error('Compiled selection mode is invalid');
  if (engine.selectionTint !== undefined && !vec3(engine.selectionTint)) throw new Error('Compiled selection tint is invalid');
  if (engine.selectionStrength !== undefined && !finite(engine.selectionStrength)) throw new Error('Compiled selection strength is invalid');

  for (const raw of world.connectors as unknown[]) {
    const connector = object(raw, 'compiled connector');
    if (!nonEmpty(connector.id) || !nonEmpty(connector.type) ||
        !nonEmpty(connector.fromLevel) || !ids.has(String(connector.fromLevel)) ||
        !nonEmpty(connector.toLevel) || !ids.has(String(connector.toLevel)) ||
        !vec3(connector.fromPosition) || !vec3(connector.toPosition) ||
        !Array.isArray(connector.forwardTraversal) || !Array.isArray(connector.reverseTraversal) ||
        !(connector.forwardTraversal as unknown[]).every(vec3) ||
        !(connector.reverseTraversal as unknown[]).every(vec3) ||
        ![0,1].includes(Number(connector.bidirectional))) {
      throw new Error(`Compiled connector ${String(connector.id)} is invalid`);
    }
  }
  const behaviourIds = new Set<string>();
  for (const raw of world.behaviours as unknown[]) {
    const behaviour = validateCompiledBehaviour(raw);
    if (behaviourIds.has(behaviour.id)) throw new Error(`Duplicate compiled behaviour ${behaviour.id}`);
    behaviourIds.add(behaviour.id);
  }
  return world as unknown as CompiledWorldDocument;
}

export function validateCompiledWorldGraph(
  world: CompiledWorldDocument,
  levels: CompiledLevelDocument[]
): void {
  if (world.levels.length !== levels.length) throw new Error('Compiled world level count mismatch');
  const entityIds = new Set<string>();
  for (let index = 0; index < levels.length; ++index) {
    if (world.levels[index].id !== levels[index].id) throw new Error('Compiled world level order mismatch');
    for (const entity of levels[index].entities) {
      if (entityIds.has(entity.id)) throw new Error(`Duplicate compiled world entity ${entity.id}`);
      entityIds.add(entity.id);
    }
  }
  for (const behaviour of world.behaviours) {
    const referenced = behaviour.type === 'oscillating-gate'
      ? [behaviour.leftEntity, behaviour.rightEntity]
      : [behaviour.entity];
    for (const id of referenced) if (!entityIds.has(id)) throw new Error(`Compiled behaviour ${behaviour.id} references missing entity ${id}`);
  }
}
