import type {
  CharacterEntityDefinition,
  ConnectorDefinition,
  DynamicBodyEntityDefinition,
  FloorHoleDefinition,
  GroundRectangle,
  LevelDocument,
  PointLightDefinition,
  PrimitiveGeometryDefinition,
  PrimitiveType,
  RoomDefinition,
  StaircaseDefinition,
  SpawnDefinition
} from '../world/documents';
import { FunctionalCommand, type EditorCommand } from './CommandHistory';
import type { EditorSelection } from './Selection';
import type { EditableSourceProject } from './SourceProjectIO';

export type LocalAddKind =
  | 'ground'
  | PrimitiveType
  | 'room'
  | 'floor-hole'
  | 'staircase'
  | 'character'
  | 'dynamic-body'
  | 'spawn'
  | 'connector'
  | 'light';

export type AuthoringOperation = {
  command: EditorCommand;
  selection: EditorSelection;
};

function nextId(prefix: string, existing: Iterable<string>): string {
  const used = new Set(existing);
  if (!used.has(prefix)) return prefix;
  for (let suffix = 2; suffix < 1_000_000; ++suffix) {
    const candidate = `${prefix}-${suffix}`;
    if (!used.has(candidate)) return candidate;
  }
  throw new Error(`Unable to allocate a stable id for ${prefix}`);
}

function addArrayItem<T extends { id: string }>(
  array: T[],
  item: T,
  label: string,
  selection: EditorSelection
): AuthoringOperation {
  return {
    command: new FunctionalCommand(
      label,
      () => {
        if (array.some(existing => existing.id === item.id)) {
          throw new Error(`Cannot add duplicate id ${item.id}`);
        }
        array.push(item);
      },
      () => {
        const index = array.findIndex(existing => existing.id === item.id);
        if (index >= 0) array.splice(index, 1);
      }
    ),
    selection
  };
}

export function selectedLevel(
  project: EditableSourceProject,
  selection: EditorSelection | null
): LevelDocument {
  if (project.kind === 'level') return project.document;
  const selectedId = selection?.levelId ??
    (selection?.kind === 'level' ? selection.id : undefined) ??
    project.document.settings.defaultLevel;
  const level = project.levels.find(candidate => candidate.id === selectedId);
  if (!level) throw new Error(`Selected level ${selectedId} does not exist`);
  return level;
}

export type LocalPlacement = { x: number; y: number; z?: number };

export function createLocalAddOperation(
  project: EditableSourceProject,
  selection: EditorSelection | null,
  kind: LocalAddKind,
  placement?: LocalPlacement
): AuthoringOperation {
  const level = selectedLevel(project, selection);
  const levelId = level.id;
  const px = placement?.x ?? 0;
  const py = placement?.y ?? 0;
  const pz = placement?.z ?? 0;
  const firstMaterial = Object.keys(level.localMaterials)[0];
  if (!firstMaterial) throw new Error(`Level ${level.id} has no material available for new geometry`);

  if (kind === 'ground') {
    const id = nextId('ground', level.ground.map(value => value.id));
    const item: GroundRectangle = {
      id,
      type: 'rectangle',
      centre: [px, py, pz],
      size: [8, 8],
      walkable: true,
      material: firstMaterial
    };
    return addArrayItem(level.ground, item, 'add ground', { kind: 'ground', id, levelId });
  }

  const primitiveKinds = new Set<PrimitiveType>([
    'cube', 'sphere', 'cone', 'pyramid', 'dodecahedron', 'icosahedron'
  ]);
  if (primitiveKinds.has(kind as PrimitiveType)) {
    const type = kind as PrimitiveType;
    const id = nextId(type, level.geometry.map(value => value.id));
    const item: PrimitiveGeometryDefinition = {
      id,
      type,
      position: [px, py, pz + 0.5],
      size: 1,
      material: firstMaterial,
      solid: true
    };
    return addArrayItem(level.geometry, item, `add ${type}`, { kind: 'geometry', id, levelId });
  }

  if (kind === 'room') {
    const id = nextId('room', (level.rooms ?? []).map(value => value.id));
    const item: RoomDefinition = {
      id,
      centre: [px, py, pz],
      width: 4,
      depth: 4,
      floorZ: 0,
      wallHeight: 2.4,
      wallThickness: 0.15,
      wallMaterial: level.settings.wallMaterial
    };
    level.rooms ??= [];
    return addArrayItem(level.rooms, item, 'add room', { kind: 'room', id, levelId });
  }

  if (kind === 'floor-hole') {
    const holes = level.floorHoles ?? (level.floorHoles = []);
    const id = nextId('floor-hole', holes.map(value => value.id));
    const item: FloorHoleDefinition = {
      id,
      type: 'rectangle',
      minimum: [px - 0.5, py - 0.5],
      maximum: [px + 0.5, py + 0.5]
    };
    return addArrayItem(holes, item, 'add floor hole', { kind: 'floor-hole', id, levelId });
  }

  if (kind === 'staircase') {
    const stairs = level.staircases ?? (level.staircases = []);
    const id = nextId('staircase', stairs.map(value => value.id));
    const item: StaircaseDefinition = {
      id,
      centreX: px,
      startY: py - 1,
      endY: py + 1,
      startZ: pz,
      endZ: pz + 2.2,
      width: 1
    };
    return addArrayItem(stairs, item, 'add staircase', { kind: 'staircase', id, levelId });
  }

  if (kind === 'character') {
    const id = nextId('character', level.entities.map(value => value.id));
    const item: CharacterEntityDefinition = {
      id,
      components: {
        transform: { position: [px, py, pz], forward: [0, 1, 0] },
        character: { controllable: true, npc: false }
      }
    };
    return addArrayItem(level.entities, item, 'add character', { kind: 'entity', id, levelId });
  }

  if (kind === 'dynamic-body') {
    const id = nextId('object', level.entities.map(value => value.id));
    const item: DynamicBodyEntityDefinition = {
      id,
      components: {
        transform: { position: [px, py, pz] },
        collider: {
          type: 'box',
          minimum: [-0.5, -0.5, 0],
          maximum: [0.5, 0.5, 1],
          solid: true
        },
        dynamicBody: {}
      }
    };
    return addArrayItem(level.entities, item, 'add dynamic body', { kind: 'entity', id, levelId });
  }

  if (kind === 'spawn') {
    const id = nextId('spawn', level.spawns.map(value => value.id));
    const item: SpawnDefinition = {
      id,
      transform: { position: [px, py, pz], forward: [0, 1, 0] },
      tags: []
    };
    return addArrayItem(level.spawns, item, 'add spawn', { kind: 'spawn', id, levelId });
  }

  if (kind === 'connector') {
    const id = nextId('connector', level.connectors.map(value => value.id));
    const item: ConnectorDefinition = {
      id,
      type: 'path',
      fromPosition: [px, py, pz],
      toPosition: [px + 1, py, pz],
      forwardTraversal: [],
      reverseTraversal: [],
      bidirectional: true
    };
    return addArrayItem(level.connectors, item, 'add connector', { kind: 'connector', id, levelId });
  }

  const id = nextId('light', level.lights.map(value => value.id));
  const item: PointLightDefinition = {
    id,
    type: 'point',
    position: [px, py, placement ? pz + 4 : 6],
    enabled: true
  };
  return addArrayItem(level.lights, item, 'add light', { kind: 'light', id, levelId });
}

type LocalCollectionItem = { id: string };

function selectedCollection(
  level: LevelDocument,
  selection: EditorSelection
): LocalCollectionItem[] | undefined {
  switch (selection.kind) {
    case 'ground': return level.ground;
    case 'entity': return level.entities;
    case 'geometry': return level.geometry;
    case 'room': return level.rooms;
    case 'floor-hole': return level.floorHoles;
    case 'staircase': return level.staircases;
    case 'room-connection': return level.roomConnections;
    case 'spawn': return level.spawns;
    case 'connector': return level.connectors;
    case 'light': return level.lights;
    default: return undefined;
  }
}

function clone<T>(value: T): T {
  return JSON.parse(JSON.stringify(value)) as T;
}

export function createDuplicateOperation(
  project: EditableSourceProject,
  selection: EditorSelection
): AuthoringOperation {
  const level = selectedLevel(project, selection);
  const collection = selectedCollection(level, selection);
  if (!collection) throw new Error(`${selection.kind} cannot be duplicated by this tool`);
  const original = collection.find(item => item.id === selection.id);
  if (!original) throw new Error(`Selected ${selection.kind} ${selection.id} does not exist`);
  const copy = clone(original);
  copy.id = nextId(original.id, collection.map(item => item.id));

  if ('position' in copy && Array.isArray((copy as Record<string, unknown>).position)) {
    const position = (copy as unknown as { position: [number, number, number] }).position;
    position[0] += 1;
    position[1] += 1;
  } else if ('centre' in copy && Array.isArray((copy as Record<string, unknown>).centre)) {
    const centre = (copy as unknown as { centre: [number, number, number] }).centre;
    centre[0] += 1;
    centre[1] += 1;
  } else if ('components' in copy) {
    const entity = copy as unknown as CharacterEntityDefinition | DynamicBodyEntityDefinition;
    entity.components.transform.position[0] += 1;
    entity.components.transform.position[1] += 1;
  } else if ('transform' in copy) {
    const transformed = copy as unknown as SpawnDefinition;
    transformed.transform.position[0] += 1;
    transformed.transform.position[1] += 1;
  } else if ('minimum' in copy && 'maximum' in copy) {
    const hole = copy as unknown as FloorHoleDefinition;
    hole.minimum[0] += 1;
    hole.minimum[1] += 1;
    hole.maximum[0] += 1;
    hole.maximum[1] += 1;
  } else if ('centreX' in copy && 'startY' in copy && 'endY' in copy) {
    const stair = copy as unknown as StaircaseDefinition;
    stair.centreX += 1;
    stair.startY += 1;
    stair.endY += 1;
  }

  return addArrayItem(
    collection,
    copy,
    `duplicate ${selection.kind}`,
    { ...selection, id: copy.id, levelId: level.id }
  );
}

export function createDeleteCommand(
  project: EditableSourceProject,
  selection: EditorSelection
): EditorCommand {
  const level = selectedLevel(project, selection);
  const collection = selectedCollection(level, selection);
  if (!collection) throw new Error(`${selection.kind} cannot be deleted by this tool`);
  const index = collection.findIndex(item => item.id === selection.id);
  if (index < 0) throw new Error(`Selected ${selection.kind} ${selection.id} does not exist`);

  if (selection.kind === 'entity') {
    const spawn = level.spawns.find(candidate => candidate.entityId === selection.id);
    if (spawn) throw new Error(`Entity ${selection.id} is referenced by spawn ${spawn.id}`);
  }

  const item = collection[index];
  return new FunctionalCommand(
    `delete ${selection.kind}`,
    () => {
      const current = collection.findIndex(candidate => candidate.id === item.id);
      if (current >= 0) collection.splice(current, 1);
    },
    () => {
      if (!collection.some(candidate => candidate.id === item.id)) {
        collection.splice(Math.min(index, collection.length), 0, item);
      }
    }
  );
}
